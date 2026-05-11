/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <sstream>

#include "common/om2/codegen/ast/ast_context.h"
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/om2_codegen_model_builder.h"
#include "common/om2/codegen/om2_code_printer.h"
#include "common/om2/codegen/program_generator.h"
#include "framework/common/taskdown_common.h"
#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_runtime_stub/include/faker/ge_model_builder.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/tensor_utils.h"

namespace ge {
namespace {

GeModelPtr CreateGeModelWithControlTasks() {
  auto graph = gert::ShareGraph::WhileGraph3();
  graph->TopologicalSorting();
  graph->SetGraphUnknownFlag(false);
  for (const auto &subgraph : graph->GetAllSubgraphs()) {
    if (subgraph != nullptr) {
      subgraph->SetGraphUnknownFlag(false);
    }
  }
  gert::GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  GE_ASSERT_TRUE(ge_root_model != nullptr, "[UT] ge_root_model is null");
  auto ge_model = MakeShared<GeModel>();
  ge_model->SetGraph(graph);
  GE_ASSERT_TRUE(ge_model != nullptr, "[UT] ge_model is null");
  auto compute_graph = ge_model->GetGraph();
  GE_ASSERT_TRUE(compute_graph != nullptr, "[UT] compute_graph is null");
  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &subgraph : compute_graph->GetAllSubgraphs()) {
    if (subgraph != nullptr) {
      subgraph->SetGraphUnknownFlag(false);
    }
  }

  const auto model_def = MakeShared<domi::ModelTaskDef>();
  ge_model->SetModelTaskDef(model_def);
  const auto cond_graph = graph->GetSubgraph("cond_instance");
  const auto body_graph = graph->GetSubgraph("body_instance");
  GE_ASSERT_TRUE((cond_graph != nullptr) && (body_graph != nullptr), "[UT] while subgraph is null");

  {
    const auto op_desc = cond_graph->FindNode("LabelSet_0")->GetOpDesc();
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_LABEL_SET));
    task_def->set_stream_id(op_desc->GetStreamId());
    task_def->mutable_label_set()->set_op_index(op_desc->GetId());
  }
  {
    const auto op_desc = cond_graph->FindNode("Stream_0")->GetOpDesc();
    GE_ASSERT_TRUE(AttrUtils::SetListInt(op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, {2}),
                   "[UT] set active stream list failed");
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_ACTIVE));
    task_def->set_stream_id(op_desc->GetStreamId());
    auto *stream_active_def = task_def->mutable_stream_active();
    stream_active_def->set_op_index(op_desc->GetId());
    stream_active_def->set_active_stream_id(2U);
  }
  {
    const auto op_desc = cond_graph->FindNode("SwitchByIndex")->GetOpDesc();
    const auto input_op_desc = cond_graph->FindNode("LessThan_5")->GetOpDesc();
    GeTensorDesc tensor(GeShape({1, 4, 4, 8}), FORMAT_NCHW, DT_FLOAT);
    TensorUtils::SetSize(tensor, 512);
    if (input_op_desc->GetOutputsSize() == 0U) {
      (void)input_op_desc->AddOutputDesc(tensor);
    }
    if (op_desc->GetInputsSize() == 0U) {
      (void)op_desc->AddInputDesc(tensor);
    }
    op_desc->SetInputOffset({1024});

    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_LABEL_SWITCH_BY_INDEX));
    task_def->set_stream_id(op_desc->GetStreamId());
    auto *switch_task_def = task_def->mutable_label_switch_by_index();
    switch_task_def->set_op_index(op_desc->GetId());
    switch_task_def->set_label_max(2);
  }
  {
    const auto op_desc = body_graph->FindNode("LabelSet_1")->GetOpDesc();
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_LABEL_SET));
    task_def->set_stream_id(op_desc->GetStreamId());
    task_def->mutable_label_set()->set_op_index(op_desc->GetId());
  }
  {
    const auto op_desc = body_graph->FindNode("Stream_1")->GetOpDesc();
    GE_ASSERT_TRUE(AttrUtils::SetListInt(op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, {2}),
                   "[UT] set active stream list failed");
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_ACTIVE));
    task_def->set_stream_id(op_desc->GetStreamId());
    auto *stream_active_def = task_def->mutable_stream_active();
    stream_active_def->set_op_index(op_desc->GetId());
    stream_active_def->set_active_stream_id(2U);
  }
  {
    const auto op_desc = body_graph->FindNode("LabelGoto")->GetOpDesc();

    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_LABEL_GOTO));
    task_def->set_stream_id(op_desc->GetStreamId());
    task_def->mutable_label_goto_ex()->set_op_index(op_desc->GetId());
  }
  {
    const auto op_desc = body_graph->FindNode("LabelSet_2")->GetOpDesc();
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_LABEL_SET));
    task_def->set_stream_id(op_desc->GetStreamId());
    task_def->mutable_label_set()->set_op_index(op_desc->GetId());
  }
  {
    const auto op_desc = graph->FindNode("NetOutput")->GetOpDesc();
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_RECORD));
    task_def->set_stream_id(op_desc->GetStreamId());
    task_def->set_event_id(0U);
    task_def->mutable_event_ex()->set_op_index(op_desc->GetId());
  }
  {
    const auto op_desc = graph->FindNode("While")->GetOpDesc();
    domi::TaskDef *task_def = model_def->add_task();
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
    task_def->set_stream_id(op_desc->GetStreamId());
    task_def->set_event_id(0U);
    task_def->mutable_event_ex()->set_op_index(op_desc->GetId());
  }

  int64_t max_stream_id = 0;
  for (const auto &node : graph->GetAllNodes()) {
    if ((node != nullptr) && (node->GetOpDesc() != nullptr)) {
      max_stream_id = std::max(max_stream_id, static_cast<int64_t>(node->GetOpDesc()->GetStreamId()));
    }
  }
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, 0);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, std::max<int64_t>(max_stream_id + 1, 3));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_NOTIFY_NUM, 1);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_EVENT_NUM, 1);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_LABEL_NUM, 3);

  for (const auto &node : compute_graph->GetDirectNode()) {
    if ((node == nullptr) || (node->GetOpDesc() == nullptr)) {
      continue;
    }
    auto op_desc = node->GetOpDesc();
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({2048});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }
  return ge_model;
}

const std::map<GeneratedFileIndex, std::string> kGeneratedFileNames = {
    {GeneratedFileIndex::kKernelRegistryFile, "g1_kernel_reg.cpp"},
    {GeneratedFileIndex::kResourcesFile, "g1_resources.cpp"},
    {GeneratedFileIndex::kArgsManagerFile, "g1_args_manager.cpp"},
    {GeneratedFileIndex::kLoadingAndRunningFile, "g1_load_and_run.cpp"},
    {GeneratedFileIndex::kInterfaceHeaderFile, "g1_interface.h"},
    {GeneratedFileIndex::kCMakeListsFile, "Makefile"},
};

Status ReadGeneratedArtifact(const Om2CodegenArtifacts &artifacts, const GeneratedFileIndex file_index,
                             std::string &output) {
  const auto iter = kGeneratedFileNames.find(file_index);
  GE_ASSERT_TRUE(iter != kGeneratedFileNames.end(), "[OM2] unknown generated file index: %zu",
                 static_cast<size_t>(file_index));
  for (const auto &artifact : artifacts) {
    if (artifact.file_name == iter->second) {
      output = artifact.data;
      return SUCCESS;
    }
  }
  GELOGE(FAILED, "[OM2] failed to find generated artifact: %s", iter->second.c_str());
  return FAILED;
}

class ControlTaskCodeGeneratorUt : public testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

Status BuildControlCodegenModel(const GeModelPtr &ge_model) {
  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<TaskCodeBuilderPtr> task_code_builders;
  Om2CodegenModel codegen_model;
  GE_ASSERT_SUCCESS(Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, ast, task_code_builders, codegen_model));

  Om2ConstMetas const_metas;
  Om2CodegenModelBuilder builder;
  return builder.Build(ge_model, task_code_builders, codegen_model, const_metas);
}

TEST_F(ControlTaskCodeGeneratorUt, GenerateControlTaskFiles_Ok) {
  std::cout << "=== CONTROL_STAGE: create_model ===" << std::endl;
  const auto ge_model = CreateGeModelWithControlTasks();
  ASSERT_NE(ge_model, nullptr);
  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<TaskCodeBuilderPtr> task_code_builders;
  Om2CodegenModel codegen_model;
  std::cout << "=== CONTROL_STAGE: create_task_handlers ===" << std::endl;
  ASSERT_EQ(Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, ast, task_code_builders, codegen_model), SUCCESS);

  Om2ConstMetas const_metas;
  Om2CodegenModelBuilder builder;
  std::cout << "=== CONTROL_STAGE: build_codegen_model ===" << std::endl;
  ASSERT_EQ(builder.Build(ge_model, task_code_builders, codegen_model, const_metas), SUCCESS);

  ProgramGenerator generator(ast, task_code_builders, codegen_model);
  std::cout << "=== CONTROL_STAGE: generate_program ===" << std::endl;
  Om2CodePrinter code_printer("g1");
  ASSERT_EQ(generator.GenerateProgram(code_printer), SUCCESS);
  Om2CodegenArtifacts artifacts;
  code_printer.GetOutputFiles(artifacts);

  std::cout << "=== CONTROL_STAGE: emit_interface ===" << std::endl;
  std::string header_file;
  ASSERT_EQ(ReadGeneratedArtifact(artifacts, GeneratedFileIndex::kInterfaceHeaderFile, header_file), SUCCESS);
  ASSERT_FALSE(header_file.empty());

  std::cout << "=== CONTROL_STAGE: emit_resources ===" << std::endl;
  std::string resources_file;
  ASSERT_EQ(ReadGeneratedArtifact(artifacts, GeneratedFileIndex::kResourcesFile, resources_file), SUCCESS);
  ASSERT_FALSE(resources_file.empty());

  std::cout << "=== CONTROL_STAGE: emit_load_and_run ===" << std::endl;
  std::string load_file;
  ASSERT_EQ(ReadGeneratedArtifact(artifacts, GeneratedFileIndex::kLoadingAndRunningFile, load_file), SUCCESS);
  ASSERT_FALSE(load_file.empty());

  EXPECT_NE(load_file.find("KernelLabelSwitchByIndexDistribute"), std::string::npos);
  EXPECT_NE(load_file.find("OM2_CHK_STATUS(KernelLabelSwitchByIndexDistribute("),
            std::string::npos);
  EXPECT_NE(load_file.find("OM2_CHK_STATUS(aclrtSwitchLabelByIndex(ptr, max_value, label_list, stream))"),
            std::string::npos);
  EXPECT_NE(load_file.find("OM2_CHK_STATUS(aclrtSwitchLabelByIndex(ptr, maxValue, labelList, stream))"),
            std::string::npos);
  EXPECT_NE(load_file.find("KernelLabelGotoExDistribute"), std::string::npos);
  EXPECT_NE(load_file.find("OM2_CHK_STATUS(KernelLabelGotoExDistribute("), std::string::npos);
  EXPECT_NE(load_file.find("MallocDeviceMemory"), std::string::npos);
  EXPECT_NE(load_file.find("if ((mem_type == RT_MEMORY_TS))"), std::string::npos);
  EXPECT_NE(load_file.find("OM2_CHK_STATUS(aclrtMemcpy"), std::string::npos);

  const std::string expected_header = R"(#include <iostream>
#include <cstddef>
#include <ctime>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <type_traits>
#include <array>
#include "securec.h"
#include "acl/acl.h"
#include "acl/acl_base.h"
#include "exe_graph/runtime/tensor.h"
#include "rt.h"

#define OM2_CHK_STATUS(expr, ...)            \
do {                                       \
  const aclError _chk_status = (expr);     \
  if (_chk_status != ACL_SUCCESS) {        \
    return _chk_status;                    \
  }                                        \
} while (false)

#define OM2_CHK_NOTNULL(ptr, ...)            \
do {                                       \
  if ((ptr) == nullptr) {                  \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define OM2_CHK_TRUE(expr, ...)              \
do {                                       \
  if (!(expr)) {                           \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define OM2_CHK_RT(expr, ...)               \
do {                                       \
  const rtError_t _rt_err = (expr);        \
  if (_rt_err != RT_ERROR_NONE) {          \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define GET_ADDR(mem_ptr, offset)   \
(reinterpret_cast<void *>(                 \
  reinterpret_cast<uintptr_t>(mem_ptr) +   \
  static_cast<uintptr_t>(offset)))

#define OM2_MAKE_GUARD(var, callback) const ::om2::ScopeGuard const_guard_##var(callback)

template<typename T>
inline T *PtrAdd(T *const ptr, const size_t max_buf_len, const size_t idx) {
  if ((ptr != nullptr) && (idx < max_buf_len)) {
    return reinterpret_cast<T *>(ptr + idx);
  }
  return nullptr;
}
template<typename TI, typename TO>
inline TO *PtrToPtr(TI *const ptr) {
  return reinterpret_cast<TO *>(ptr);
}
inline uint64_t PtrToValue(const void *const ptr) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
}
inline void *ValueToPtr(const uint64_t value) {
  return reinterpret_cast<void *>(static_cast<uintptr_t>(value));
}

template<typename... Args>
inline std::vector<uint64_t> FlattenHostArgs(Args&&... args) {
  std::vector<uint64_t> buf;
  auto append_arg = [&](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_pointer_v<T>) {
      buf.push_back(reinterpret_cast<uint64_t>(arg));
    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
      for (auto d : arg) buf.push_back(static_cast<uint64_t>(d));
    } else if constexpr (std::is_integral_v<T>) {
      buf.push_back(static_cast<uint64_t>(arg));
    } else {
      static_assert(sizeof(T) == 0, "Unsupported type in FlattenHostArgs");
    }
  };
  (append_arg(std::forward<Args>(args)), ...);
  return buf;
}

namespace om2 {
constexpr int32_t INPUT_NUM = 1;
constexpr int32_t OUTPUT_NUM = 1;
typedef void *Om2ModelHandle;

struct BinDataInfo {
  const void *data;
  size_t size;
};

struct AicpuParamHead {
  uint32_t length;
  uint32_t ioAddrNum;
  uint32_t extInfoLength;
  uint64_t extInfoAddr;
};

struct AicpuSessionInfo {
  uint64_t sessionId;
  uint64_t kernelId;
  bool sessFlag;
};

struct ArgsInfo {
  void *host_addr;
  void *dev_addr;
  size_t size;
};

class ScopeGuard {
  public:
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    explicit ScopeGuard(const std::function<void()> &on_exit_scope)
        : on_exit_scope_(on_exit_scope) {}

    ~ScopeGuard() {
      if (on_exit_scope_) {
        try {
          on_exit_scope_();
        } catch (std::bad_function_call &) {
        } catch (...) {
        }
      }
    }

  private:
    std::function<void()> on_exit_scope_;
};

class Om2ArgsTable {
  public:
    Om2ArgsTable() = default;
    ~Om2ArgsTable();
    aclError Init();
    ArgsInfo * GetArgsInfo(size_t index);
    void * GetDevArgAddr(size_t offset);
    void * GetHostArgAddr(size_t offset);
    aclError UpdateHostArgs(size_t index, const uintptr_t addr);
    aclError CopyArgsToDevice();
  private:
    int64_t args_size_;
    std::vector<ArgsInfo> args_info_;
    std::vector<uint8_t> host_args_;
    void *dev_args_;
    std::vector<std::vector<void *>> iow_args_addrs_;
};

class Om2Model {
  public:
    Om2Model(const char **bin_files, const void **bin_data, size_t *bin_size, size_t bin_num, void **constants, void *work_ptr, uint64_t *session_id);
    ~Om2Model();
    aclError InitResources();
    aclError RegisterKernels();
    aclError Load();
    aclError Run(size_t input_count, void **input_data, size_t output_count, void **output_data);
    aclError RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data);
    aclError ReleaseResources();
  private:
    void **constants_;
    aclmdlRI model_handle_;
    std::vector<aclrtStream> stream_list_;
    std::vector<aclrtNotify> notify_list_;
    std::vector<aclrtEvent> event_list_;
    std::vector<aclrtLabel> label_list_;
    aclrtLabelList aclrt_label_list_;
    std::vector<aclrtLabel> label_used_;
    aclError CreateLabelListForLabelSwitch(uint32_t op_index, std::vector<uint32_t> label_list_indexs);
    std::map<uint32_t, aclrtLabelList> label_switch_label_list_;
    aclError CreateLabelListForLabelGotoEx(uint32_t op_index, uint32_t label_index);
    std::vector<void *> label_goto_ex_index_values_;
    std::map<uint32_t, std::pair<void *, uint32_t>> label_goto_args_;
    std::map<uint32_t, aclrtLabelList> label_goto_ex_label_list_;
    void *total_dev_mem_ptr_;
    bool is_stream_list_bind_;
    std::unordered_map<std::string, BinDataInfo> bin_info_map_;
    Om2ArgsTable args_table_;
    uint64_t *session_id_;
    uint64_t kernel_id_;
    std::vector<void *> dev_ext_info_mem_ptrs_;
    std::map<uint32_t, void *> mem_event_id_mem_map_;
    void *overflow_addr_;
    std::vector<void *> dev_dynamic_mem_ptrs_;
    void *session_scope_mem_ptr_;
};
} // namespace om2
#ifdef __cplusplus
extern "C" {
#endif

aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id);

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data);

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data);

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle);

#ifdef __cplusplus
}
#endif
)";
  const std::string expected_resources = R"(#include "_interface.h"

namespace om2 {
Om2Model::Om2Model(const char **bin_files, const void **bin_data, size_t *bin_size, size_t bin_num, void **constants, void *work_ptr, uint64_t *session_id)
  : constants_(constants), total_dev_mem_ptr_(work_ptr), session_id_(session_id), kernel_id_(0), session_scope_mem_ptr_(nullptr) {
  for (size_t i = 0; (i < bin_num); ++i) {
    bin_info_map_[std::string(bin_files[i])] = {bin_data[i], bin_size[i]};
  }
  stream_list_.resize(3);
  notify_list_.resize(1);
  event_list_.resize(1);
  label_list_.resize(3);
}

Om2Model::~Om2Model() {
  (void)ReleaseResources();
}

aclError Om2Model::InitResources() {
  // 1. 创建 model
  OM2_CHK_STATUS(aclmdlRIBuildBegin(&model_handle_, 0));

  // 2. 获取overflow地址
  OM2_CHK_STATUS(aclrtCtxGetFloatOverflowAddr(&overflow_addr_));

  // 3. 创建其他资源
  // 创建下沉Stream并绑定模型
  uint32_t stream0_flag = RT_STREAM_PERSISTENT;
  OM2_CHK_RT(rtStreamCreateWithFlags(&stream_list_[0], 0, stream0_flag));
  auto bind0_flag = RT_HEAD_STREAM;
  OM2_CHK_RT(rtModelBindStream(model_handle_, stream_list_[0], bind0_flag));
  uint32_t stream1_flag = RT_STREAM_PERSISTENT;
  OM2_CHK_RT(rtStreamCreateWithFlags(&stream_list_[1], 0, stream1_flag));
  auto bind1_flag = RT_HEAD_STREAM;
  OM2_CHK_RT(rtModelBindStream(model_handle_, stream_list_[1], bind1_flag));
  uint32_t stream2_flag = RT_STREAM_PERSISTENT;
  OM2_CHK_RT(rtStreamCreateWithFlags(&stream_list_[2], 0, stream2_flag));
  auto bind2_flag = RT_HEAD_STREAM;
  OM2_CHK_RT(rtModelBindStream(model_handle_, stream_list_[2], bind2_flag));
  is_stream_list_bind_ = true;
  // 创建Notify
  for (size_t i = 0; (i < 1); ++i) {
    OM2_CHK_STATUS(aclrtCreateNotify(&notify_list_[i], ACL_NOTIFY_DEVICE_USE_ONLY));
  }
  // 创建Event
  for (size_t i = 0; (i < 1); ++i) {
    OM2_CHK_STATUS(aclrtCreateEventWithFlag(&event_list_[i], ACL_EVENT_SYNC | ACL_EVENT_CAPTURE_STREAM_PROGRESS | ACL_EVENT_TIME_LINE));
  }
  // 创建Label
  for (size_t i = 0; (i < 3); ++i) {
    OM2_CHK_STATUS(aclrtCreateLabel(&label_list_[i]));
  }
  OM2_CHK_STATUS(CreateLabelListForLabelSwitch(6, {1, 2}));
  OM2_CHK_STATUS(CreateLabelListForLabelGotoEx(11, 0));
  args_table_.Init();
  return ACL_SUCCESS;
}

aclError Om2Model::ReleaseResources() {
  for (auto label : label_list_) {
    if ((label != nullptr)) {
      OM2_CHK_STATUS(aclrtDestroyLabel(label));
    }
  }
  for (auto event : event_list_) {
    OM2_CHK_STATUS(aclrtDestroyEvent(event));
  }
  for (auto notify : notify_list_) {
    OM2_CHK_STATUS(aclrtDestroyNotify(notify));
  }
  if (is_stream_list_bind_) {
    for (auto stream : stream_list_) {
      OM2_CHK_STATUS(aclmdlRIUnbindStream(model_handle_, stream));
    }
  }
  for (auto stream : stream_list_) {
    OM2_CHK_STATUS(aclrtDestroyStream(stream));
  }
  for (auto &label : label_switch_label_list_) {
    if ((label.second != nullptr)) {
      OM2_CHK_STATUS(aclrtDestroyLabelList(label.second));
    }
  }
  for (auto &label_goto_ex_index_value : label_goto_ex_index_values_) {
    OM2_CHK_STATUS(aclrtFree(label_goto_ex_index_value));
  }
  for (auto &label_goto_arg : label_goto_args_) {
    void *arg_addr = label_goto_arg.second.first;
    if ((arg_addr != nullptr)) {
      OM2_CHK_STATUS(aclrtDestroyLabelList(arg_addr));
    }
  }
  label_goto_args_.clear();
  OM2_CHK_STATUS(aclmdlRIDestroy(model_handle_));
  if ((session_scope_mem_ptr_ != nullptr)) {
    OM2_CHK_STATUS(aclrtFree(session_scope_mem_ptr_));
  }
  for (int i = 0; (i < dev_ext_info_mem_ptrs_.size()); i++) {
    if ((dev_ext_info_mem_ptrs_[i] != nullptr)) {
      OM2_CHK_STATUS(aclrtFree(dev_ext_info_mem_ptrs_[i]));
    }
  }
  for (int i = 0; (i < dev_dynamic_mem_ptrs_.size()); i++) {
    if ((dev_dynamic_mem_ptrs_[i] != nullptr)) {
      OM2_CHK_STATUS(aclrtFree(dev_dynamic_mem_ptrs_[i]));
    }
  }
  return ACL_SUCCESS;
}

aclError Om2Model::CreateLabelListForLabelSwitch(uint32_t op_index, std::vector<uint32_t> label_list_indexs)
{
  label_switch_label_list_[op_index] = nullptr;
  uint32_t label_used_size = label_list_indexs.size();
  label_used_.resize(label_used_size, nullptr);
  for (uint32_t i = 0; i < label_used_size; i++) {
    label_used_[i] = label_list_[label_list_indexs[i]];
  }
  OM2_CHK_STATUS(aclrtCreateLabelList(label_used_.data(), label_used_.size(), &(label_switch_label_list_[op_index])));
  return ACL_SUCCESS;
}

aclError Om2Model::CreateLabelListForLabelGotoEx(uint32_t op_index, uint32_t label_index)
{
  label_goto_ex_label_list_[op_index] = nullptr;
  const auto it = label_goto_args_.find(label_index);
  if (it != label_goto_args_.cend()) {
    label_goto_ex_label_list_[op_index] = it->second.first;
  } else {
    label_used_.resize(1, nullptr);
    label_used_[0] = label_list_[static_cast<size_t>(label_index)];
    OM2_CHK_STATUS(aclrtCreateLabelList(label_used_.data(), label_used_.size(), &(label_goto_ex_label_list_[op_index])));
    label_goto_args_[label_index] = {label_goto_ex_label_list_[op_index], static_cast<uint32_t>(sizeof(rtLabelDevInfo))};
  }
  return ACL_SUCCESS;
}
} // namespace om2
)";
  ASSERT_EQ(header_file, expected_header);
  ASSERT_EQ(resources_file, expected_resources);
}

TEST_F(ControlTaskCodeGeneratorUt, BuildControlTaskSemantics_LabelSwitchListSizeMismatch_Fail) {
  const auto ge_model = CreateGeModelWithControlTasks();
  ASSERT_NE(ge_model, nullptr);
  const auto cond_graph = ge_model->GetGraph()->GetSubgraph("cond_instance");
  ASSERT_NE(cond_graph, nullptr);
  const auto switch_node = cond_graph->FindNode("SwitchByIndex");
  ASSERT_NE(switch_node, nullptr);
  const auto switch_op = switch_node->GetOpDesc();
  ASSERT_NE(switch_op, nullptr);

  ASSERT_TRUE(AttrUtils::SetListInt(switch_op, ATTR_NAME_LABEL_SWITCH_LIST, {1}));
  EXPECT_NE(BuildControlCodegenModel(ge_model), SUCCESS);
}

TEST_F(ControlTaskCodeGeneratorUt, BuildControlTaskSemantics_LabelSwitchLabelIdOutOfRange_Fail) {
  const auto ge_model = CreateGeModelWithControlTasks();
  ASSERT_NE(ge_model, nullptr);
  const auto cond_graph = ge_model->GetGraph()->GetSubgraph("cond_instance");
  ASSERT_NE(cond_graph, nullptr);
  const auto switch_node = cond_graph->FindNode("SwitchByIndex");
  ASSERT_NE(switch_node, nullptr);
  const auto switch_op = switch_node->GetOpDesc();
  ASSERT_NE(switch_op, nullptr);

  ASSERT_TRUE(AttrUtils::SetListInt(switch_op, ATTR_NAME_LABEL_SWITCH_LIST, {1, 3}));
  EXPECT_NE(BuildControlCodegenModel(ge_model), SUCCESS);
}

TEST_F(ControlTaskCodeGeneratorUt, BuildControlTaskSemantics_LabelGotoLabelIdOutOfRange_Fail) {
  const auto ge_model = CreateGeModelWithControlTasks();
  ASSERT_NE(ge_model, nullptr);
  const auto body_graph = ge_model->GetGraph()->GetSubgraph("body_instance");
  ASSERT_NE(body_graph, nullptr);
  const auto label_goto_node = body_graph->FindNode("LabelGoto");
  ASSERT_NE(label_goto_node, nullptr);
  const auto label_goto_op = label_goto_node->GetOpDesc();
  ASSERT_NE(label_goto_op, nullptr);

  ASSERT_TRUE(AttrUtils::SetInt(label_goto_op, ATTR_NAME_LABEL_SWITCH_INDEX, 3));
  EXPECT_NE(BuildControlCodegenModel(ge_model), SUCCESS);
}

}  // namespace
}  // namespace ge
