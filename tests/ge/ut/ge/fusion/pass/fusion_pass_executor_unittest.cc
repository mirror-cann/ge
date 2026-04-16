/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "common/share_graph.h"
#include "es_ge_test_ops.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"
#include "nlohmann/json.hpp"

#include "stub/gert_runtime_stub.h"
#include "ge/fusion/pattern.h"
#include "common/topo_checker.h"
#include "ge/fusion/pass/fusion_pass_reg.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "register/custom_pass_context_impl.h"
#include "graph/ge_local_context.h"
#include "compiler/graph/fusion/pass/fusion_pass_executor.h"
#include "register/optimization_option_registry.h"

// 单例，为了保证ut效果，需要清理其成员
#define private public
#include "compiler/graph/fusion/pass/pass_registry.h"
#include "ge/fusion/pass/decompose_pass.h"
#undef private
#include "compiler/graph/fusion/pass/python_fusion_base_pass_adapter.h"
#include "compiler/graph/fusion/pass/python_fusion_base_pass_pybind_bridge.h"

namespace ge {
namespace fusion {
namespace {
std::string GetCodeDir() {
  ge::char_t current_path[MMPA_MAX_PATH] = {'\0'};
  getcwd(current_path, MMPA_MAX_PATH);
  return current_path;
}

constexpr const char *kEnvPythonPassPath = "ASCEND_GE_PY_PASS_PATH";

class ScopedTempDir {
 public:
  ScopedTempDir() {
    char dir_template[] = "/tmp/ge_python_pass_ut_XXXXXX";
    const auto *created_dir = mkdtemp(dir_template);
    dir_path_ = (created_dir == nullptr) ? std::string() : std::string(created_dir);
  }

  ~ScopedTempDir() {
    if (dir_path_.empty()) {
      return;
    }
    for (const auto &file_name : created_files_) {
      (void)remove((dir_path_ + "/" + file_name).c_str());
    }
    (void)rmdir(dir_path_.c_str());
  }

  std::string FilePath(const std::string &file_name) const {
    return dir_path_ + "/" + file_name;
  }

  std::string CreateFilePath(const std::string &file_name) {
    TrackFile(file_name);
    return FilePath(file_name);
  }

  void TrackFile(const std::string &file_name) {
    created_files_.push_back(file_name);
  }

 private:
  std::string dir_path_;
  std::vector<std::string> created_files_;
};

class ScopedEnvVar {
 public:
  ScopedEnvVar(const std::string &name, const std::string &value) : name_(name) {
    const char *old_value = getenv(name_.c_str());
    if (old_value != nullptr) {
      has_old_value_ = true;
      old_value_ = old_value;
    }
    (void)setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvVar() {
    if (has_old_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  std::string old_value_;
  bool has_old_value_{false};
};

void WriteFile(const std::string &file_path, const std::string &content) {
  std::ofstream file(file_path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file << content;
  file.close();
}

std::string ReadFile(const std::string &file_path) {
  std::ifstream file(file_path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

ScopedTempDir &GetSharedPybindPassDir() {
  static ScopedTempDir dir;
  return dir;
}

const std::string &GetSharedPybindPassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_shared_passes.py");
  return path;
}

const std::string &GetSharedPybindMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("bridge_success_marker.txt");
  return path;
}

void EnsureSharedPybindPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.graph import Graph\n"
              << "from ge.passes import FusionBasePass, PassStage, register_fusion_pass\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindBridgePass', stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindBridgePass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        assert isinstance(graph, Graph)\n"
              << "        Path(MARKER_FILE).write_text(f\"{graph.name}|{context.pass_name}\", encoding='utf-8')\n"
              << "        return 0\n\n"
              << "@register_fusion_pass(name='PythonPybindBridgeFailedPass', "
                 "stage=PassStage.AFTER_BUILTIN_FUSION_PASS)\n"
              << "class PythonPybindBridgeFailedPass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        raise RuntimeError('python bridge run failed')\n";
    WriteFile(GetSharedPybindPassFilePath(), pass_code.str());
  });
}

struct PythonFusionBasePassRuntimeSnapshot {
  int create_count{0};
  int destroy_count{0};
  int run_count{0};
  Status run_status{SUCCESS};
  std::string last_descriptor_key;
  std::string last_pass_name;
  std::string last_context_pass_name;
  std::string last_graph_name;
};

struct PythonFusionBasePassHolderForUt {
  std::string descriptor_key;
  std::string pass_name;
};

PythonFusionBasePassRuntimeSnapshot g_python_fusion_base_runtime_snapshot;

void ResetPythonFusionBasePassRuntimeSnapshot() {
  g_python_fusion_base_runtime_snapshot = {};
}

void *CreatePythonFusionBasePassHolderForUt(const PythonPassDescriptor *pass_desc) {
  ++g_python_fusion_base_runtime_snapshot.create_count;
  if (pass_desc == nullptr) {
    return nullptr;
  }
  auto *holder = new (std::nothrow) PythonFusionBasePassHolderForUt{pass_desc->descriptor_key, pass_desc->pass_name};
  return holder;
}

void DestroyPythonFusionBasePassHolderForUt(void *holder) {
  ++g_python_fusion_base_runtime_snapshot.destroy_count;
  delete static_cast<PythonFusionBasePassHolderForUt *>(holder);
}

Status RunPythonFusionBasePassHolderForUt(void *holder, GraphPtr &graph, CustomPassContext &pass_context) {
  ++g_python_fusion_base_runtime_snapshot.run_count;
  const auto *typed_holder = static_cast<PythonFusionBasePassHolderForUt *>(holder);
  if (typed_holder != nullptr) {
    g_python_fusion_base_runtime_snapshot.last_descriptor_key = typed_holder->descriptor_key;
    g_python_fusion_base_runtime_snapshot.last_pass_name = typed_holder->pass_name;
  }
  g_python_fusion_base_runtime_snapshot.last_context_pass_name = pass_context.GetPassName().GetString();
  g_python_fusion_base_runtime_snapshot.last_graph_name = graph->GetName();
  return g_python_fusion_base_runtime_snapshot.run_status;
}
} // namespace
using namespace ge::es;
class UtestFusionPassExecutor : public testing::Test {
 public:
  static void TearDownTestSuite() {
    // Python bridge 的显式 dlclose 放到 suite 结束时做一次，
    // 避免每个用例都把进程级 bridge 热卸载掉。
    ShutdownPythonFusionBasePassesForProcess();
  }

  void SetUp() override {
    PreparePythonPathForSt();
    (void)unsetenv(kEnvPythonPassPath);
    UnloadPythonFusionBasePasses();
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ClearPythonFusionBasePassRuntimeRegistry();
    ResetPythonFusionBasePassRuntimeSnapshot();
    global_options_bak_ = ge::GetThreadLocalContext().GetAllGlobalOptions();
    session_options_bak_ = ge::GetThreadLocalContext().GetAllSessionOptions();
    graph_options_bak_ = ge::GetThreadLocalContext().GetAllGraphOptions();
  }
  void TearDown() override {
    (void)unsetenv(kEnvPythonPassPath);
    UnloadPythonFusionBasePasses();
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ClearPythonFusionBasePassRuntimeRegistry();
    ResetPythonFusionBasePassRuntimeSnapshot();

    GetThreadLocalContext().SetGlobalOption(global_options_bak_);
    GetThreadLocalContext().SetSessionOption(session_options_bak_);
    GetThreadLocalContext().SetGraphOption(graph_options_bak_);
    GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
    RestorePythonPathForSt();
  }
 private:
  void PreparePythonPathForSt() {
#ifdef ST_FUSION_PASS_PY_INSTALL_DIR
    const char *old_python_path = getenv("PYTHONPATH");
    if (old_python_path != nullptr) {
      has_python_path_bak_ = true;
      python_path_bak_ = old_python_path;
      if (python_path_bak_.find(ST_FUSION_PASS_PY_INSTALL_DIR) == std::string::npos) {
        const std::string new_python_path = std::string(ST_FUSION_PASS_PY_INSTALL_DIR) + ":" + python_path_bak_;
        (void)setenv("PYTHONPATH", new_python_path.c_str(), 1);
      }
      return;
    }
    (void)setenv("PYTHONPATH", ST_FUSION_PASS_PY_INSTALL_DIR, 1);
#endif
  }

  void RestorePythonPathForSt() {
#ifdef ST_FUSION_PASS_PY_INSTALL_DIR
    if (has_python_path_bak_) {
      (void)setenv("PYTHONPATH", python_path_bak_.c_str(), 1);
    } else {
      (void)unsetenv("PYTHONPATH");
    }
    python_path_bak_.clear();
    has_python_path_bak_ = false;
#endif
  }

  std::map<std::string, std::string> global_options_bak_;
  std::map<std::string, std::string> graph_options_bak_;
  std::map<std::string, std::string> session_options_bak_;
  std::string python_path_bak_;
  bool has_python_path_bak_{false};
};
/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(1, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(2, {{"Relu"}}), "success");
    }
    if (node->GetName() == "x_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
    }
    if (node->GetName() == "y_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}}), "success");
    }
    if (node->GetType() == "Relu") {
      std::cout << node->GetName() << std::endl;
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_OptionToOff) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  GetThreadLocalContext().GetOo().Initialize({{ge::OPTIMIZATION_SWITCH, "TransDataToReluPass:off"}},
                                            OptionRegistry::GetInstance().GetRegisteredOptTable());
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_FALSE(has_relu); // 图没有被改
}
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_ConfigToOff) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  std::string fusion_config_json_str = "{\n"
      "      \"Switch\":{\n"
      "          \"GraphFusion\":{\n"
      "            \"TransDataToReluPass\" : \"off\"\n"
      "          },\n"
      "          \"UBFusion\":{\n"
      "          }\n"
      "      }}";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << fusion_config_json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_FALSE(has_relu); // 图没有被改
  remove("./fusion_switch_config.json");
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_ConfigAllOff_PassToOn) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  std::string fusion_config_json_str = "{\n"
      "      \"Switch\":{\n"
      "          \"GraphFusion\":{\n"
      "            \"TransDataToReluPass\" : \"on\"\n"
      "            \"ALL\" : \"off\"\n"
      "          },\n"
      "          \"UBFusion\":{\n"
      "          }\n"
      "      }}";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << fusion_config_json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_TRUE(has_relu); // 图被改
  remove("./fusion_switch_config.json");
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_ConfigAllOff) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  std::string fusion_config_json_str = "{\n"
      "      \"Switch\":{\n"
      "          \"GraphFusion\":{\n"
      "            \"ALL\" : \"off\"\n"
      "          },\n"
      "          \"UBFusion\":{\n"
      "          }\n"
      "      }}";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << fusion_config_json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_FALSE(has_relu); // 图没有被改
  remove("./fusion_switch_config.json");
}

/**
 * shape to relu
 */
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_WithSubgraph) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto shape = EsShape(data, DT_INT64);
      esb_graph->SetGraphOutput(shape, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::IfGraph2();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  NodePtr relu_node = nullptr;
  for (const auto &node : target_compute_graph->GetAllNodes()) {
    if (node->GetType() == "Relu") {
      relu_node = node;
    }
  }
  EXPECT_NE(relu_node, nullptr);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_WithSubgraph_Failed) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto shape = EsShape(data, DT_INT64);
      esb_graph->SetGraphOutput(shape, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override { // invalid replacement
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::IfGraph2();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_NE(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_NE(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), NOT_CHANGED);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  NodePtr relu_node = nullptr;
  for (const auto &node : target_compute_graph->GetAllNodes()) {
    if (node->GetType() == "Relu") {
      relu_node = node;
    }
  }
  EXPECT_EQ(relu_node, nullptr);
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_FAILED) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override { // invalid replacement
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  auto ret = pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape);
  EXPECT_NE(ret, SUCCESS);
  EXPECT_NE(ret, NOT_CHANGED);
}

/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestFusionPassExecutor, DecomposePass_Run_AfterInferShape) {
  // define pass
  class RunDecomposeTransDataPass : public DecomposePass {
   public:
    RunDecomposeTransDataPass(const std::vector<AscendString> &op_types) : DecomposePass(op_types) {}
   protected:
    bool MeetRequirements(const GNode &matched_node) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_DECOMPOSE_PASS(RunDecomposeTransDataPass, {"TransData"}).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(1, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(2, {{"Relu"}}), "success");
    }
    if (node->GetName() == "x_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
    }
    if (node->GetName() == "y_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}}), "success");
    }
    if (node->GetType() == "Relu") {
      std::cout << node->GetName() << std::endl;
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, DecomposePass_Run_AfterInferShape_Failed) {
  // define pass
  class RunDecomposeTransDataPass : public DecomposePass {
   public:
    RunDecomposeTransDataPass(const std::vector<AscendString> &op_type) : DecomposePass(op_type) {}
   protected:
    bool MeetRequirements(const GNode &matched_node) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };
  REG_DECOMPOSE_PASS(RunDecomposeTransDataPass, {"TransData"}).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  auto ret = pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape);
  EXPECT_NE(ret, SUCCESS);
  EXPECT_NE(ret, NOT_CHANGED);
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_Run_CreateExecuteDestroy) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.fusion.base.stage2";
  pass_desc.pass_name = "PythonFusionBaseStage2Pass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonFusionBaseStage2Pass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kFusionBase;

  PythonFusionBasePassCallbacks callbacks;
  callbacks.create = CreatePythonFusionBasePassHolderForUt;
  callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonFusionBasePass(pass_desc, callbacks));

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  {
    FusionPassExecutor pass_executor;
    EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
    EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.create_count, 1);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.run_count, 2);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.last_descriptor_key, pass_desc.descriptor_key);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.last_pass_name, pass_desc.pass_name);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.last_context_pass_name, pass_desc.pass_name);
  }
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_Run_Failed) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.fusion.base.stage2.failed";
  pass_desc.pass_name = "PythonFusionBaseStage2FailedPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonFusionBaseStage2FailedPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kFusionBase;

  PythonFusionBasePassCallbacks callbacks;
  callbacks.create = CreatePythonFusionBasePassHolderForUt;
  callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonFusionBasePass(pass_desc, callbacks));
  g_python_fusion_base_runtime_snapshot.run_status = FAILED;

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  {
    FusionPassExecutor pass_executor;
    const auto ret = pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape);
    EXPECT_EQ(ret, FAILED);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.create_count, 1);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.run_count, 1);
  }
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_PybindBridge_RunSuccess) {
  EnsureSharedPybindPassFile();
  const auto &marker_file = GetSharedPybindMarkerFilePath();
  (void)remove(marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPassFilePath());
  ASSERT_EQ(RegisterPythonFusionBasePassesFromPlugin(), SUCCESS);

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  const auto expected_graph_name = target_compute_graph->GetName();
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(ReadFile(marker_file), expected_graph_name + "|PythonPybindBridgePass");
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_PybindBridge_RunFailedOnPythonException) {
  EnsureSharedPybindPassFile();
  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPassFilePath());
  ASSERT_EQ(RegisterPythonFusionBasePassesFromPlugin(), SUCCESS);

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterBuiltinFusionPass), FAILED);
}
} // namespace fusion
} // namespace ge

// CPython 内部分配器（_PyObject_Malloc / PyThread_allocate_lock）在 Py_Finalize()
// 后仍有残余内存不被回收，这是 CPython 的已知行为，不是业务代码的泄漏。
// 通过 LSan 抑制规则让 ut_fusion_pass_executor_utest 不因此失败。
extern "C" const char *__lsan_default_suppressions() {
  return "leak:_PyObject_Malloc\n"
         "leak:PyThread_allocate_lock\n"
         "leak:_PyObject_Realloc\n";
}
