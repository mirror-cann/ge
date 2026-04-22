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
#include "compiler/graph/fusion/pass/python_pass_adapter.h"
#include "compiler/graph/fusion/pass/python_pass_pybind_bridge.h"

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

const std::string &GetSharedPybindPatternPassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_pattern_passes.py");
  return path;
}

const std::string &GetSharedPybindPatternMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pattern_bridge_success_marker.txt");
  return path;
}

const std::string &GetSharedPybindPatternMatcherConfigPassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_pattern_matcher_config_passes.py");
  return path;
}

const std::string &GetSharedPybindPatternMatcherConfigMarkerFilePath() {
  static const std::string path =
      GetSharedPybindPassDir().CreateFilePath("pattern_matcher_config_bridge_marker.txt");
  return path;
}

void EnsureSharedPybindPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.graph import Graph\n"
              << "from ge.passes import FusionBasePass, PassStage, register_fusion_pass, PassContext\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindBridgePass', stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindBridgePass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        assert isinstance(graph, Graph)\n"
              << "        Path(MARKER_FILE).write_text(f\"{graph.name}|{context.get_pass_name()}\", encoding='utf-8')\n"
              << "        return 0\n\n"
              << "@register_fusion_pass(name='PythonPybindBridgeFailedPass', "
                 "stage=PassStage.AFTER_BUILTIN_FUSION_PASS)\n"
              << "class PythonPybindBridgeFailedPass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        raise RuntimeError('python bridge run failed')\n";
    WriteFile(GetSharedPybindPassFilePath(), pass_code.str());
  });
}

void EnsureSharedPybindPatternPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.es.graph_builder import GraphBuilder\n"
              << "from es_ut_test import Add\n"
              << "from ge.passes import (\n"
              << "    PassStage,\n"
              << "    PatternFusionPass,\n"
              << "    PatternMatcherConfigBuilder,\n"
              << "    capture_tensor,\n"
              << "    create_pattern,\n"
              << "    create_replacement,\n"
              << "    register_fusion_pass,\n"
              << ")\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindPatternMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindPatternAddZeroPass', stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindPatternAddZeroPass(PatternFusionPass):\n"
              << "    def __init__(self):\n"
              << "        super().__init__(\n"
              << "            PatternMatcherConfigBuilder().enable_const_value_match().build()\n"
              << "        )\n\n"
              << "    def patterns(self):\n"
              << "        pattern_builder = GraphBuilder('add_zero_pattern')\n"
              << "        data = pattern_builder.create_input(0)\n"
              << "        zero = pattern_builder.create_scalar_float(0.0)\n"
              << "        add = Add(data, zero)\n"
              << "        pattern_builder.set_graph_output(add, 0)\n"
              << "        pattern = create_pattern(pattern_builder.build_and_reset())\n"
              << "        pattern.capture_tensor(capture_tensor(data))\n"
              << "        return [pattern]\n\n"
              << "    def meet_requirements(self, match_result):\n"
              << "        return True\n\n"
              << "    def replacement(self, match_result):\n"
              << "        captured = match_result.get_captured_tensor(0)\n"
              << "        Path(MARKER_FILE).write_text(\n"
              << "            f\"{match_result.get_pattern_graph_name()}|{captured.node.name}:{captured.index}\",\n"
              << "            encoding='utf-8'\n"
              << "        )\n"
              << "        replacement_builder = GraphBuilder('add_zero_replacement')\n"
              << "        passthrough = replacement_builder.create_input(0)\n"
              << "        replacement_builder.set_graph_output(passthrough, 0)\n"
              << "        return create_replacement(replacement_builder.build_and_reset())\n";
    WriteFile(GetSharedPybindPatternPassFilePath(), pass_code.str());
  });
}

void EnsureSharedPybindPatternMatcherConfigPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.es.graph_builder import GraphBuilder\n"
              << "from es_ut_test import Add\n"
              << "from ge.passes import (\n"
              << "    PassStage,\n"
              << "    PatternFusionPass,\n"
              << "    PatternMatcherConfigBuilder,\n"
              << "    create_pattern,\n"
              << "    create_replacement,\n"
              << "    register_fusion_pass,\n"
              << ")\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindPatternMatcherConfigMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindPatternMatcherConfigPass', "
                 "stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindPatternMatcherConfigPass(PatternFusionPass):\n"
              << "    def __init__(self):\n"
              << "        super().__init__(\n"
              << "            PatternMatcherConfigBuilder().enable_const_value_match().build()\n"
              << "        )\n\n"
              << "    def patterns(self):\n"
              << "        pattern_builder = GraphBuilder('const_one_pattern')\n"
              << "        data = pattern_builder.create_input(0)\n"
              << "        one = pattern_builder.create_const_float(1.0)\n"
              << "        add = Add(data, one)\n"
              << "        pattern_builder.set_graph_output(add, 0)\n"
              << "        return [create_pattern(pattern_builder.build_and_reset())]\n\n"
              << "    def replacement(self, match_result):\n"
              << "        Path(MARKER_FILE).write_text(match_result.get_pattern_graph_name(), encoding='utf-8')\n"
              << "        replacement_builder = GraphBuilder('const_one_replacement')\n"
              << "        passthrough = replacement_builder.create_input(0)\n"
              << "        replacement_builder.set_graph_output(passthrough, 0)\n"
              << "        return create_replacement(replacement_builder.build_and_reset())\n";
    WriteFile(GetSharedPybindPatternMatcherConfigPassFilePath(), pass_code.str());
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

struct PatternFusionPassRuntimeSnapshot {
  int create_count{0};
  int destroy_count{0};
  int patterns_count{0};
  int meet_requirements_count{0};
  int replacement_count{0};
  Status patterns_status{SUCCESS};
  bool meet_requirements_result{true};
  Status replacement_status{SUCCESS};
};

PatternFusionPassRuntimeSnapshot g_pattern_fusion_runtime_snapshot;

void ResetPatternFusionRuntimeSnapshot() {
  g_pattern_fusion_runtime_snapshot = {};
}

void *CreatePatternFusionPassHolderForUt(const PythonPassDescriptor *pass_desc) {
  ++g_pattern_fusion_runtime_snapshot.create_count;
  if (pass_desc == nullptr) {
    return nullptr;
  }
  return new (std::nothrow) PythonFusionBasePassHolderForUt{pass_desc->descriptor_key, pass_desc->pass_name};
}

void DestroyPatternFusionPassHolderForUt(void *holder) {
  ++g_pattern_fusion_runtime_snapshot.destroy_count;
  delete static_cast<PythonFusionBasePassHolderForUt *>(holder);
}

Status GetPatternsForUt(void *holder, std::vector<PatternUniqPtr> &patterns) {
  ++g_pattern_fusion_runtime_snapshot.patterns_count;
  return g_pattern_fusion_runtime_snapshot.patterns_status;
}

bool MeetRequirementsForUt(void *holder, const std::unique_ptr<MatchResult> &match_result) {
  ++g_pattern_fusion_runtime_snapshot.meet_requirements_count;
  return g_pattern_fusion_runtime_snapshot.meet_requirements_result;
}

Status ReplacementForUt(void *holder, const std::unique_ptr<MatchResult> &match_result,
                        GraphUniqPtr &replacement_graph) {
  ++g_pattern_fusion_runtime_snapshot.replacement_count;
  return g_pattern_fusion_runtime_snapshot.replacement_status;
}

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
  void SetUp() override {
    PreparePythonPathForSt();
    (void)unsetenv(kEnvPythonPassPath);
    ShutdownPythonPassesForProcess();
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ClearPythonPassRuntimeRegistry();
    ResetPythonFusionBasePassRuntimeSnapshot();
    ResetPatternFusionRuntimeSnapshot();
    global_options_bak_ = ge::GetThreadLocalContext().GetAllGlobalOptions();
    session_options_bak_ = ge::GetThreadLocalContext().GetAllSessionOptions();
    graph_options_bak_ = ge::GetThreadLocalContext().GetAllGraphOptions();
  }
  void TearDown() override {
    (void)unsetenv(kEnvPythonPassPath);
    ShutdownPythonPassesForProcess();
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ClearPythonPassRuntimeRegistry();
    ResetPythonFusionBasePassRuntimeSnapshot();
    ResetPatternFusionRuntimeSnapshot();

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

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePythonFusionBasePassHolderForUt;
  callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

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

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePythonFusionBasePassHolderForUt;
  callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));
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
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);

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
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterBuiltinFusionPass), FAILED);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_PybindBridge_RunSuccess) {
  EnsureSharedPybindPatternPassFile();
  const auto &marker_file = GetSharedPybindPatternMarkerFilePath();
  (void)remove(marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPatternPassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);

  auto target_graph = ge::es::EsGraphBuilder("python_pattern_target");
  auto *esb_graph = target_graph.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto zero = EsCreateScalarFloat(esb_graph, 0.0f);
  auto add = EsAdd(data, zero);
  esb_graph->SetGraphOutput(add, 0);
  auto target_ge_graph = target_graph.BuildAndReset();
  auto target_compute_graph = GraphUtilsEx::GetComputeGraph(*target_ge_graph);
  ASSERT_NE(target_compute_graph, nullptr);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(ReadFile(marker_file), "add_zero_pattern|input_0:0");

  uint32_t add_node_count = 0;
  uint32_t const_node_count = 0;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Add") {
      ++add_node_count;
    }
    if (node->GetType() == CONSTANT) {
      ++const_node_count;
    }
  }
  EXPECT_EQ(add_node_count, 0U);
  EXPECT_EQ(const_node_count, 0U);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_PybindBridge_MatcherConfigTakesEffect) {
  EnsureSharedPybindPatternMatcherConfigPassFile();
  const auto &marker_file = GetSharedPybindPatternMatcherConfigMarkerFilePath();
  (void)remove(marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPatternMatcherConfigPassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);

  auto target_graph = ge::es::EsGraphBuilder("python_pattern_matcher_config_target");
  auto *esb_graph = target_graph.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto zero = EsCreateScalarFloat(esb_graph, 0.0f);
  auto add = EsAdd(data, zero);
  esb_graph->SetGraphOutput(add, 0);
  auto target_ge_graph = target_graph.BuildAndReset();
  auto target_compute_graph = GraphUtilsEx::GetComputeGraph(*target_ge_graph);
  ASSERT_NE(target_compute_graph, nullptr);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(access(marker_file.c_str(), F_OK), -1);

  uint32_t add_node_count = 0;
  uint32_t const_node_count = 0;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Add") {
      ++add_node_count;
    }
    if (node->GetType() == CONSTANT) {
      ++const_node_count;
    }
  }
  EXPECT_EQ(add_node_count, 1U);
  EXPECT_EQ(const_node_count, 1U);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_CreateAndDestroy) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.pattern.fusion.create";
  pass_desc.pass_name = "PythonPatternFusionCreatePass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonPatternFusionCreatePass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kPatternFusion;

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePatternFusionPassHolderForUt;
  callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  callbacks.patterns = GetPatternsForUt;
  callbacks.replacement = ReplacementForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  {
    auto adapter = std::make_unique<PythonPatternFusionPassAdapter>(pass_desc);
    ASSERT_TRUE(adapter->IsValid());
    EXPECT_EQ(g_pattern_fusion_runtime_snapshot.create_count, 1);

    // Patterns callback should be invoked
    auto patterns = adapter->Patterns();
    EXPECT_EQ(g_pattern_fusion_runtime_snapshot.patterns_count, 1);
  }
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_MeetRequirements_DefaultFallback) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.pattern.fusion.meetreq";
  pass_desc.pass_name = "PythonPatternFusionMeetReqPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonPatternFusionMeetReqPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kPatternFusion;

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePatternFusionPassHolderForUt;
  callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  callbacks.patterns = GetPatternsForUt;
  callbacks.replacement = ReplacementForUt;
  // meet_requirements is nullptr -> should fall back to PatternFusionPass::MeetRequirements (return true)

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  auto adapter = std::make_unique<PythonPatternFusionPassAdapter>(pass_desc);
  ASSERT_TRUE(adapter->IsValid());

  // meet_requirements callback is nullptr -> falls back to PatternFusionPass::MeetRequirements (returns true)
  auto null_result = std::unique_ptr<MatchResult>();
  EXPECT_TRUE(adapter->MeetRequirements(null_result));

  // meet_requirements callback is nullptr, so count should stay 0
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.meet_requirements_count, 0);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_InvalidOnMissingRequiredCallbacks) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.pattern.fusion.invalid";
  pass_desc.pass_name = "PythonPatternFusionInvalidPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonPatternFusionInvalidPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kPatternFusion;

  // Missing patterns and replacement -> IsValid() should return false from callbacks validation
  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePatternFusionPassHolderForUt;
  callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  // patterns and replacement are nullptr

  EXPECT_FALSE(callbacks.IsValid(pass_desc.kind));
  EXPECT_FALSE(RegisterPythonPass(pass_desc, callbacks));
}

TEST_F(UtestFusionPassExecutor, PythonPassHolder_ReusedAcrossAdapters) {
  // Verify PythonPassHolder lifecycle works for FusionBase
  PythonPassDescriptor base_desc;
  base_desc.descriptor_key = "python.holder.base";
  base_desc.pass_name = "PythonHolderBasePass";
  base_desc.module_name = "python.pass.sample";
  base_desc.class_name = "PythonHolderBasePass";
  base_desc.stage = CustomPassStage::kAfterInferShape;
  base_desc.kind = PythonPassKind::kFusionBase;

  PythonFusionPassCallbacks base_callbacks;
  base_callbacks.create = CreatePythonFusionBasePassHolderForUt;
  base_callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  base_callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonPass(base_desc, base_callbacks));

  // Verify PythonPassHolder lifecycle works for PatternFusion
  PythonPassDescriptor pattern_desc;
  pattern_desc.descriptor_key = "python.holder.pattern";
  pattern_desc.pass_name = "PythonHolderPatternPass";
  pattern_desc.module_name = "python.pass.sample";
  pattern_desc.class_name = "PythonHolderPatternPass";
  pattern_desc.stage = CustomPassStage::kAfterInferShape;
  pattern_desc.kind = PythonPassKind::kPatternFusion;

  PythonFusionPassCallbacks pattern_callbacks;
  pattern_callbacks.create = CreatePatternFusionPassHolderForUt;
  pattern_callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  pattern_callbacks.patterns = GetPatternsForUt;
  pattern_callbacks.replacement = ReplacementForUt;

  ASSERT_TRUE(RegisterPythonPass(pattern_desc, pattern_callbacks));

  // Both adapters should be valid and independently manageable
  auto base_adapter = std::make_unique<PythonFusionBasePassAdapter>(base_desc);
  auto pattern_adapter = std::make_unique<PythonPatternFusionPassAdapter>(pattern_desc);
  EXPECT_TRUE(base_adapter->IsValid());
  EXPECT_TRUE(pattern_adapter->IsValid());

  // Verify independent create/destroy
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.create_count, 1);
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.create_count, 1);

  base_adapter.reset();
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.destroy_count, 1);
  // Pattern adapter should still be valid
  EXPECT_TRUE(pattern_adapter->IsValid());

  pattern_adapter.reset();
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.destroy_count, 1);
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
