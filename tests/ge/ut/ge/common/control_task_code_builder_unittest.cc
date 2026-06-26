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

std::string GetInterfaceDumpApisExpected() {
  return R"(
struct Om2TaskIoEntry {
  const struct Om2Tensor *tensor;
  uint64_t offset;
};

enum Om2L0ArgKind {
    OM2_L0_ARG_INPUT = 0,
    OM2_L0_ARG_OUTPUT = 1,
    OM2_L0_ARG_WORKSPACE = 2,
    OM2_L0_ARG_TILING = 3,
    OM2_L0_ARG_SHAPE_INFO = 4,
    OM2_L0_ARG_LEVEL1_DESC = 5,
    OM2_L0_ARG_PLACEHOLDER = 6,
    OM2_L0_ARG_CUSTOM_VALUE = 7,
    OM2_L0_ARG_FFTS_ADDR = 8,
    OM2_L0_ARG_EVENT_ADDR = 9,
    OM2_L0_ARG_OVERFLOW_ADDR = 10,
    OM2_L0_ARG_EMPTY_ADDR = 11
};

struct Om2L0ArgSlotInfo {
    uint32_t kind;
    uint32_t flags;
    uint64_t args_offset;
    uint64_t value;
    uint32_t related_index;
    uint32_t event_id;
    uint64_t level1_target_offset;
};

struct Om2L0TaskRawInfo {
    uint32_t version;
    uint32_t need_assert_or_printf;
    uint64_t arg_num;
    const struct Om2L0ArgSlotInfo* args;
};

struct Om2TaskInfo {
  const char* op_name;
  const char* op_type;
  uint32_t task_id;
  uint32_t stream_id;
  uint32_t context_id;
  uint32_t thread_id;
  uint32_t block_dim;
  uint64_t op_desc_id;
  uintptr_t args_base;
  uint64_t args_size;
  uint64_t input_num;
  const struct Om2TaskIoEntry* inputs;
  uint32_t output_num;
  const struct Om2TaskIoEntry* outputs;
  uint32_t workspace_num;
  const uint64_t* workspace_addrs;
  const uint64_t* workspace_sizes;
  uint32_t task_type;
  void* stream;
  uint32_t is_raw_address;
  const struct Om2L0TaskRawInfo* l0_exception_dump_info;
};

extern "C" {
__attribute__((weak)) int32_t ReportDfxTaskPreprocess(uint32_t model_id,
                                                       void* instance_handle,
                                                       const struct Om2TaskInfo* task_info,
                                                       const void* extended_attrs,
                                                       size_t extended_attrs_size);

__attribute__((weak)) int32_t ReportDfxTaskPostprocess(uint32_t model_id,
                                                        void* instance_handle,
                                                        const struct Om2TaskInfo* task_info,
                                                        const void* extended_attrs,
                                                        size_t extended_attrs_size);

__attribute__((weak)) int32_t IsDataDumpEnabled(uint32_t model_id,
                                                      void* instance_handle,
                                                      const char* op_name,
                                                      uint8_t* is_data_dump);
}
)";
}

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
  const auto ge_model = CreateGeModelWithControlTasks();
  ASSERT_NE(ge_model, nullptr);
  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<TaskCodeBuilderPtr> task_code_builders;
  Om2CodegenModel codegen_model;
  ASSERT_EQ(Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, ast, task_code_builders, codegen_model), SUCCESS);

  Om2ConstMetas const_metas;
  Om2CodegenModelBuilder builder;
  ASSERT_EQ(builder.Build(ge_model, task_code_builders, codegen_model, const_metas), SUCCESS);

  ProgramGenerator generator(ast, task_code_builders, codegen_model);
  Om2CodePrinter code_printer("g1");
  ASSERT_EQ(generator.GenerateProgram(code_printer), SUCCESS);
  Om2CodegenArtifacts artifacts;
  code_printer.GetOutputFiles(artifacts);

  std::string header_file;
  ASSERT_EQ(ReadGeneratedArtifact(artifacts, GeneratedFileIndex::kInterfaceHeaderFile, header_file), SUCCESS);
  ASSERT_FALSE(header_file.empty());

  std::string resources_file;
  ASSERT_EQ(ReadGeneratedArtifact(artifacts, GeneratedFileIndex::kResourcesFile, resources_file), SUCCESS);
  ASSERT_FALSE(resources_file.empty());

  std::string load_file;
  ASSERT_EQ(ReadGeneratedArtifact(artifacts, GeneratedFileIndex::kLoadingAndRunningFile, load_file), SUCCESS);
  ASSERT_FALSE(load_file.empty());

  EXPECT_NE(load_file.find("KernelLabelSwitchByIndexDistribute"), std::string::npos);
  EXPECT_NE(load_file.find("OM2_CHK_STATUS(KernelLabelSwitchByIndexDistribute("), std::string::npos);
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
#include "dlog_pub.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <cinttypes>

// OM2 Logging Macros
#define OM2_MODULE_NAME static_cast<int32_t>(GE)
#define OM2_LOG_HEADER "OM2"

static inline uint64_t Om2GetTid() {
#ifdef __GNUC__
  return static_cast<uint64_t>(syscall(__NR_gettid));
#else
  return static_cast<uint64_t>(GetCurrentThreadId());
#endif
}

static inline bool Om2IsLogEnable(int32_t level) {
  return CheckLogLevel(OM2_MODULE_NAME, level) == 1;
}

#define OM2_LOGD(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_DEBUG)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_DEBUG, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_LOGI(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_INFO)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_INFO, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_LOGW(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_WARN)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_WARN, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_LOGE(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_ERROR)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_ERROR, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_CHK_STATUS(expr, ...)            \
do {                                       \
  const aclError _chk_status = (expr);     \
  if (_chk_status != ACL_SUCCESS) {        \
    OM2_LOGE(__VA_ARGS__);                 \
    return _chk_status;                    \
  }                                        \
} while (false)

#define OM2_CHK_NOTNULL(ptr, ...)            \
do {                                       \
  if ((ptr) == nullptr) {                  \
    OM2_LOGE(__VA_ARGS__);                 \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define OM2_CHK_TRUE(expr, ...)              \
do {                                       \
  if (!(expr)) {                           \
    OM2_LOGE(__VA_ARGS__);                 \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define OM2_CHK_RT(expr, ...)               \
do {                                       \
  const rtError_t _rt_err = (expr);        \
  if (_rt_err != RT_ERROR_NONE) {          \
    OM2_LOGE(__VA_ARGS__);                 \
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
inline uint64_t PtrToU64(const void *ptr) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
}

inline void *ResolveOpAddr(uint32_t mem_src, uint64_t offset,
                          void *total_dev_mem_ptr, void *session_scope_mem_ptr,
                          void **constants) {
  void *base_ptr;
  if (mem_src == 0xFFFFFFFFU) {
    base_ptr = session_scope_mem_ptr;
  } else if (mem_src == 0U) {
    base_ptr = total_dev_mem_ptr;
  } else {
    base_ptr = constants[mem_src - 1U];
  }
  return GET_ADDR(base_ptr, offset);
}

extern "C" {
struct Om2Tensor {
  uint64_t device_address;
  uint64_t size;
  int32_t data_type;
  int32_t format;
  const int64_t* shape_dims;
  uint64_t shape_dims_num;
};
}

template<typename... Args>
inline std::vector<uint64_t> FlattenHostArgs(Args&&... args) {
  std::vector<uint64_t> buf;
  auto append_arg = [&](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_pointer_v<T>) {
      buf.push_back(reinterpret_cast<uint64_t>(arg));
    } else if constexpr (std::is_same_v<T, Om2Tensor>) {
      buf.push_back(arg.device_address);
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

struct Om2TaskIoEntry {
  const struct Om2Tensor *tensor;
  uint64_t offset;
};

enum Om2L0ArgKind {
    OM2_L0_ARG_INPUT = 0,
    OM2_L0_ARG_OUTPUT = 1,
    OM2_L0_ARG_WORKSPACE = 2,
    OM2_L0_ARG_TILING = 3,
    OM2_L0_ARG_SHAPE_INFO = 4,
    OM2_L0_ARG_LEVEL1_DESC = 5,
    OM2_L0_ARG_PLACEHOLDER = 6,
    OM2_L0_ARG_CUSTOM_VALUE = 7,
    OM2_L0_ARG_FFTS_ADDR = 8,
    OM2_L0_ARG_EVENT_ADDR = 9,
    OM2_L0_ARG_OVERFLOW_ADDR = 10,
    OM2_L0_ARG_EMPTY_ADDR = 11
};

struct Om2L0ArgSlotInfo {
    uint32_t kind;
    uint32_t flags;
    uint64_t args_offset;
    uint64_t value;
    uint32_t related_index;
    uint32_t event_id;
    uint64_t level1_target_offset;
};

struct Om2L0TaskRawInfo {
    uint32_t version;
    uint32_t need_assert_or_printf;
    uint64_t arg_num;
    const struct Om2L0ArgSlotInfo* args;
};

struct Om2TaskInfo {
  const char* op_name;
  const char* op_type;
  uint32_t task_id;
  uint32_t stream_id;
  uint32_t context_id;
  uint32_t thread_id;
  uint32_t block_dim;
  uint64_t op_desc_id;
  uintptr_t args_base;
  uint64_t args_size;
  uint64_t input_num;
  const struct Om2TaskIoEntry* inputs;
  uint32_t output_num;
  const struct Om2TaskIoEntry* outputs;
  uint32_t workspace_num;
  const uint64_t* workspace_addrs;
  const uint64_t* workspace_sizes;
  uint32_t task_type;
  void* stream;
  uint32_t is_raw_address;
  const struct Om2L0TaskRawInfo* l0_exception_dump_info;
};

extern "C" {
__attribute__((weak)) int32_t ReportDfxTaskPreprocess(uint32_t model_id,
                                                       void* instance_handle,
                                                       const struct Om2TaskInfo* task_info,
                                                       const void* extended_attrs,
                                                       size_t extended_attrs_size);

__attribute__((weak)) int32_t ReportDfxTaskPostprocess(uint32_t model_id,
                                                        void* instance_handle,
                                                        const struct Om2TaskInfo* task_info,
                                                        const void* extended_attrs,
                                                        size_t extended_attrs_size);

__attribute__((weak)) int32_t IsDataDumpEnabled(uint32_t model_id,
                                                      void* instance_handle,
                                                      const char* op_name,
                                                      uint8_t* is_data_dump);
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

struct TfAiCpuExInfo {
  uint32_t fwkKernelType;
  uint32_t fwkOperateType;
  uint64_t sessionID;
  uint64_t stepIDAddr;
  uint64_t kernelID;
  uint64_t nodeDefLen;
  uint64_t nodeDefBuf;
  uint64_t funDefLibLen;
  uint64_t funDefLibBuf;
  uint64_t inputOutputLen;
  uint64_t inputOutputBuf;
  uint64_t workspaceBaseAddr;
  uint64_t inputOutputAddr;
  uint64_t extInfoLen;
  uint64_t extInfoAddr;
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

// 算子分发类型枚举
enum OpDispatchType : uint32_t {
    DISPATCH_AICORE = 0,        // AI Core / AICPU 算子执行
};

// 算子参数类型枚举
enum OpArgType : int32_t {
    OP_ARG_INPUT = 0,          // 输入张量
    OP_ARG_OUTPUT = 1,         // 输出张量
    OP_ARG_WORKSPACE = 2,      // 工作空间
    OP_ARG_CONST_TENSOR = 3,   // 常量张量
    OP_ARG_LEVEL1_DESC = 4,    // Level1 描述符
    OP_ARG_SHAPE_INFO = 5,     // Shape 信息
    OP_ARG_CUSTOM_VALUE = 6,   // 自定义值
    OP_ARG_PLACEHOLDER = 7,    // 占位符
    OP_ARG_OPTIONAL_EMPTY = 8, // 可选空参数
    OP_ARG_FFTS_ADDR = 9,      // FFTS 地址
    OP_ARG_EVENT_ADDR = 10,    // 事件地址
    OP_ARG_OVERFLOW_ADDR = 11, // 溢出地址
    OP_ARG_TILING = 12,        // Tiling 数据
    OP_ARG_RAW_ADDR = 13,      // 原始地址
};

// 算子参数信息结构体
struct OpArgInfo {
  int32_t type;                      // 参数类型（OpArgType，switch 条件）
  struct {                           // 地址解析（INPUT/OUTPUT/WORKSPACE/CONST_TENSOR）
    uint32_t mem_src;                // 内存来源（0=设备内存，0xFFFFFFFF=session，≥1=常量数组索引）
    uint64_t offset;                 // 内存偏移量
  } addr;
  union {
    struct {                         // Tensor 元数据（INPUT/OUTPUT）
      uint64_t size;                 // 数据大小（字节），WORKSPACE 也用此字段
      int32_t data_type;             // 数据类型
      int32_t format;                // 数据格式
      int64_t shape[8];              // Shape 维度数组（最多8维）
      uint32_t num_shape_dims;       // 实际 shape 维度数
      uint64_t args_offset;          // args table 内字节偏移
    } tensor;
    uint64_t custom_value;           // 自定义值（LEVEL1_DESC/SHAPE_INFO/CUSTOM_VALUE/EVENT_ADDR）
    struct {                         // Tiling 数据（TILING）
      const uint8_t *raw_data;       // 原始数据指针
      uint32_t raw_data_len;         // 原始数据长度
    } tiling;
  } data;
};

// 算子定义结构体
struct OpDef {
  OpDispatchType dispatch_type;   // 分发类型，决定走哪个 case 分支
  const char *op_name;            // 算子名称，用于日志和 Report 上报
  const OpArgInfo *argsInfo;           // 参数信息数组，地址解析时遍历
  // 各 task 专属数据下沉到 union（后续适配其他 task 时再增加对应成员）
  union {
    struct {                      // AICORE 专属 (kernel_type=0)
      const char *op_type;        // 算子类型名，用于 Report 上报
      uint32_t num_io_addrs;      // IO 地址数量，控制地址解析循环次数
      uint32_t args_idx;          // 参数表索引，用于 GetArgsInfo 查找
      struct {                    // Launch 配置，构建 LaunchKernelConfig → AssembleLaunchConfig
        uint8_t schedule_mode;    // 调度模式
        uint32_t engine_type;     // 引擎类型
        uint32_t block_dim_offset;// BlockDim 偏移量
        bool is_block_task_prefetch; // 是否预取 Block Task
        uint16_t time_out;        // 超时时间
        uint32_t local_memory_size;  // 本地内存大小
      } launch;
      struct {                    // Kernel 执行参数，传给 KernelTaskDistribute
        uint32_t block_dim;       // Block 维度
        uint32_t func_idx;        // 函数句柄索引，用于查找 func_handles
      } kernel;
      struct {                    // L0 信息，构建 Om2L0TaskRawInfo
        uint32_t need_assert_or_printf; // 是否需要 assert/printf
        uint32_t num_l0_slots;    // L0 slot 数量
        const Om2L0ArgSlotInfo *l0_slots; // L0 slot 信息数组
      } l0;
    } aicore;
    struct {  // AICPU 专属（共享 DISPATCH_AICORE, kernel_type=1）
      uint32_t kernel_type;
      uint32_t args_idx;
      uint32_t num_io_addrs;
      uint32_t func_idx;
      uint32_t block_dim;
      uint8_t schedule_mode;
      uint32_t engine_type;
      uint32_t block_dim_offset;
      bool is_block_task_prefetch;
      uint16_t time_out;
      uint32_t local_memory_size;
      uint32_t ext_info_len;
      int32_t session_info_offset;
      uint32_t aicpu_task_index;
      uint32_t task_type;
    } aicpu;
  } task_data;
};

// 算子分发上下文结构体
struct DispatchOpContext {
  void *total_dev_mem_ptr;   // 设备总内存指针
  void *session_scope_mem_ptr; // Session 作用域内存指针
  void **constants;          // 常量数组
  Om2ArgsTable &args_table;  // 参数表
  aclrtFuncHandle *func_handles; // 函数句柄数组
  aclrtStream stream;        // 执行流
  uint32_t model_id;         // 模型 ID
  void *instance_handle;     // 实例句柄
  std::map<uint32_t, void *> &event_id_mem_map; // 事件 ID 到内存的映射
  std::vector<void *> &dev_dynamic_mem_ptrs; // 设备动态内存指针列表
  void *overflow_addr;       // 溢出地址
};


class Om2Model {
  public:
    Om2Model(const char **bin_files, const void **bin_data, size_t *bin_size, size_t bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle);
    ~Om2Model();
    aclError InitResources();
    aclError RegisterKernels();
    aclError Load();
    aclmdlRI GetRtModelHandle();
    aclError Run(size_t input_count, void **input_data, size_t output_count, void **output_data, int32_t stream_sync_timeout);
    aclError RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data);
    aclError ReleaseResources();
  private:
    void **constants_;
    aclmdlRI model_handle_;
    std::vector<aclrtFuncHandle> func_handles_;
    std::vector<aclrtStream> stream_list_;
    std::vector<aclrtNotify> notify_list_;
    std::vector<aclrtEvent> event_list_;
    std::vector<aclrtLabel> label_list_;
    aclrtLabelList aclrt_label_list_;
    std::vector<aclrtLabel> label_used_;
    std::map<uint32_t, aclrtLabelList> label_switch_label_list_;
    std::map<uint32_t, std::pair<void *, uint32_t>> label_goto_args_;
    std::map<uint32_t, aclrtLabelList> label_goto_ex_label_list_;
    aclError CreateLabelListForLabelSwitch(uint32_t op_index, std::vector<uint32_t> label_list_indexs);
    aclError CreateLabelListForLabelGotoEx(uint32_t op_index, uint32_t label_index);
    std::vector<void *> label_goto_ex_index_values_;
    void *total_dev_mem_ptr_;
    bool is_stream_list_bind_;
    std::unordered_map<std::string, BinDataInfo> bin_info_map_;
    Om2ArgsTable args_table_;
    uint64_t *session_id_;
    uint32_t model_id_;
    void *instance_handle_;
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

aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, aclmdlRI *rt_model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle);

aclError Om2ModelLoad(om2::Om2ModelHandle *model_handle);

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data);

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout);

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle);

#ifdef __cplusplus
}
#endif
)";
  const std::string expected_resources = R"(#line 1 "g1_resources.cpp"
#include "_interface.h"

namespace om2 {
Om2Model::Om2Model(const char **bin_files, const void **bin_data, size_t *bin_size, size_t bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle)
  : constants_(constants), total_dev_mem_ptr_(work_ptr), session_id_(session_id), model_id_(model_id), instance_handle_(instance_handle), kernel_id_(0), session_scope_mem_ptr_(nullptr) {
  for (size_t i = 0; (i < bin_num); ++i) {
    bin_info_map_[std::string(bin_files[i])] = {bin_data[i], bin_size[i]};
  }
  stream_list_.resize(3);
  notify_list_.resize(1);
  event_list_.resize(1);
  label_list_.resize(3);
  OM2_LOGD("Om2Model created");
}

Om2Model::~Om2Model() {
  OM2_LOGD("~Om2Model");
  (void)ReleaseResources();
}

aclError Om2Model::InitResources() {
  OM2_LOGI("InitResources begin");
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
  OM2_LOGI("InitResources done");
  return ACL_SUCCESS;
}

aclError Om2Model::ReleaseResources() {
  OM2_LOGI("ReleaseResources begin");
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
  OM2_LOGI("ReleaseResources done");
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
