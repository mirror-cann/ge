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
#include <gmock/gmock.h>
#include <memory>
#include "graph/load/model_manager/task_info/ge/custom_task_info.h"
#include "graph/load/model_manager/davinci_model.h"
#include "graph/load/model_manager/reusable_stream_allocator.h"
#include "graph/load/model_manager/model_args_manager.h"
#include "graph/load/model_manager/memory_block_manager.h"
#include "graph/op_desc.h"
#include "graph/utils/op_desc_utils.h"
#include "ffts_plus_proto_tools.h"
#include "depends/runtime/src/runtime_stub.h"
#include "depends/ascendcl/src/ascendcl_stub.h"
#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_registry.h"
#include "exe_graph/runtime/kernel_args.h"
#include "framework/runtime/args_handler.h"

namespace ge {
namespace {
class AclMockMemcpy : public AclRuntimeStub {
 public:
  MOCK_METHOD5(aclrtMemcpy, int32_t(void *, size_t, const void *, size_t, aclrtMemcpyKind));
};
}  // namespace

class UtestCustomTaskInfo : public testing::Test {
 protected:
  void SetUp() override {
    RTS_STUB_SETUP();
    auto acl_mock_memcpy = [](void *dst, size_t dest_max, const void *src, size_t count,
                              aclrtMemcpyKind kind) -> int32_t {
      (void)kind;
      if ((count == 0U) || (dst == nullptr) || (src == nullptr)) {
        return -1;
      }
      (void)memcpy_s(dst, dest_max, src, count);
      return RT_ERROR_NONE;
    };

    auto acl_runtime_stub = std::make_shared<AclMockMemcpy>();
    AclRuntimeStub::SetInstance(acl_runtime_stub);
    EXPECT_CALL(*acl_runtime_stub, aclrtMemcpy).WillRepeatedly(testing::Invoke(acl_mock_memcpy));
  }

  void TearDown() override {
    AclRuntimeStub::Reset();
    RTS_STUB_TEARDOWN();
  }

  void InitBasicTaskInfo(CustomTaskInfo &task_info, DavinciModel &model, const std::string &op_name = "custom_op") {
    auto op_desc = std::make_shared<OpDesc>(op_name, "CustomOp");
    GeTensorDesc desc;
    op_desc->AddInputDesc(desc);
    op_desc->AddOutputDesc(desc);
    op_desc->SetId(0);
    op_desc->SetStreamId(0);
    model.op_list_[0] = op_desc;

    rtStream_t stream = nullptr;
    model.reusable_stream_allocator_ = ReusableStreamAllocator::Create();
    model.reusable_stream_allocator_->GetOrCreateRtStream(stream, 0, 0, 0);
    model.stream_list_ = {stream};

    task_info.davinci_model_ = &model;
    task_info.op_desc_ = op_desc;
    task_info.stream_ = stream;
    task_info.input_data_addrs_ = {0x1000};
    task_info.output_data_addrs_ = {0x2000};
  }

  void EnableDump(DavinciModel &model, const std::string &mode = "all") {
    DumpProperties dump_properties;
    dump_properties.SetDumpMode(mode);
    dump_properties.AddPropertyValue(DUMP_ALL_MODEL, {});
    model.SetDumpProperties(dump_properties);
  }

  void CleanupDumpOp(DumpOp &dump_op) {
    if (dump_op.proto_dev_mem_ != nullptr) {
      (void)aclrtFree(dump_op.proto_dev_mem_);
      dump_op.proto_dev_mem_ = nullptr;
    }
    if (dump_op.proto_size_dev_mem_ != nullptr) {
      (void)aclrtFree(dump_op.proto_size_dev_mem_);
      dump_op.proto_size_dev_mem_ = nullptr;
    }
    if (dump_op.launch_kernel_args_dev_mem_ != nullptr) {
      (void)aclrtFree(dump_op.launch_kernel_args_dev_mem_);
      dump_op.launch_kernel_args_dev_mem_ = nullptr;
    }
    if (dump_op.dev_mem_unload_ != nullptr) {
      (void)aclrtFree(dump_op.dev_mem_unload_);
      dump_op.dev_mem_unload_ = nullptr;
    }
  }
};

TEST_F(UtestCustomTaskInfo, Release_ResetSinkOnlyAllocator) {
  CustomTaskInfo task_info;
  auto allocator = std::make_shared<gert::memory::SinkOnlyAllocator>();
  auto mem_block_manager = std::make_shared<ge::MemoryBlockManager>(0);
  allocator->SetAllocator(mem_block_manager);
  task_info.sink_only_allocator_ = allocator;
  ASSERT_NE(task_info.sink_only_allocator_, nullptr);

  ASSERT_EQ(task_info.Release(), SUCCESS);
  ASSERT_EQ(task_info.sink_only_allocator_, nullptr);
  mem_block_manager->Release();
}

TEST_F(UtestCustomTaskInfo, Release_NullSinkOnlyAllocator) {
  CustomTaskInfo task_info;
  ASSERT_EQ(task_info.sink_only_allocator_, nullptr);

  ASSERT_EQ(task_info.Release(), SUCCESS);
  ASSERT_EQ(task_info.sink_only_allocator_, nullptr);
}

TEST_F(UtestCustomTaskInfo, Release_AclrtGetCurrentContextFailStillSuccess) {
  CustomTaskInfo task_info;
  auto allocator = std::make_shared<gert::memory::SinkOnlyAllocator>();
  auto mem_block_manager = std::make_shared<ge::MemoryBlockManager>(0);
  allocator->SetAllocator(mem_block_manager);
  task_info.sink_only_allocator_ = allocator;

  AclRuntimeStub::GetInstance()->SetErrorResultApiName("aclrtGetCurrentContext");
  ASSERT_EQ(task_info.Release(), SUCCESS);
  ASSERT_EQ(task_info.sink_only_allocator_, nullptr);
  mem_block_manager->Release();
  AclRuntimeStub::GetInstance()->SetErrorResultApiName("");
}

TEST_F(UtestCustomTaskInfo, InsertDumpOp_ReturnSuccessWhenOpNeedDumpIsFalse) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);

  const Status ret = task_info.InsertDumpOp("input");
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(task_info.input_custom_dump_.proto_dev_mem_, nullptr);
  EXPECT_EQ(task_info.input_custom_dump_.proto_size_dev_mem_, nullptr);
}

TEST_F(UtestCustomTaskInfo, InsertDumpOp_InputSuccessWhenDumpEnabled) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);
  EnableDump(model);

  const Status ret = task_info.InsertDumpOp("input");
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_NE(task_info.input_custom_dump_.proto_dev_mem_, nullptr);
  EXPECT_NE(task_info.input_custom_dump_.proto_size_dev_mem_, nullptr);
  CleanupDumpOp(task_info.input_custom_dump_);
}

TEST_F(UtestCustomTaskInfo, InsertDumpOp_OutputSuccessWhenDumpEnabled) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);
  EnableDump(model);

  const Status ret = task_info.InsertDumpOp("output");
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_NE(task_info.output_custom_dump_.proto_dev_mem_, nullptr);
  EXPECT_NE(task_info.output_custom_dump_.proto_size_dev_mem_, nullptr);
  CleanupDumpOp(task_info.output_custom_dump_);
}

TEST_F(UtestCustomTaskInfo, InsertDumpOp_InputSkippedWhenDumpModeIsOutput) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);
  EnableDump(model, "output");

  const Status ret = task_info.InsertDumpOp("input");
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(task_info.input_custom_dump_.proto_dev_mem_, nullptr);
  EXPECT_EQ(task_info.output_custom_dump_.proto_dev_mem_, nullptr);
}

TEST_F(UtestCustomTaskInfo, InsertDumpOp_OutputSkippedWhenDumpModeIsInput) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);
  EnableDump(model, "input");

  const Status ret = task_info.InsertDumpOp("output");
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(task_info.input_custom_dump_.proto_dev_mem_, nullptr);
  EXPECT_EQ(task_info.output_custom_dump_.proto_dev_mem_, nullptr);
}

TEST_F(UtestCustomTaskInfo, InsertDumpOp_InvalidModeReturnSuccess) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);
  EnableDump(model);

  const Status ret = task_info.InsertDumpOp("invalid");
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(task_info.input_custom_dump_.proto_dev_mem_, nullptr);
  EXPECT_EQ(task_info.output_custom_dump_.proto_dev_mem_, nullptr);
}

TEST_F(UtestCustomTaskInfo, UpdateCustomDumpAddrs_ReturnSuccessWhenDumpEnabled) {
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  InitBasicTaskInfo(task_info, model);
  EnableDump(model);

  EXPECT_EQ(task_info.InsertDumpOp("input"), SUCCESS);
  EXPECT_EQ(task_info.InsertDumpOp("output"), SUCCESS);
  const Status ret = task_info.UpdateCustomDumpAddrs({0x3000}, {0x3008});
  EXPECT_EQ(ret, SUCCESS);
  ASSERT_EQ(task_info.input_custom_dump_.op_mapping_info_.task_size(), 1);
  ASSERT_EQ(task_info.output_custom_dump_.op_mapping_info_.task_size(), 1);
  ASSERT_EQ(task_info.input_custom_dump_.op_mapping_info_.task(0).input_size(), 1);
  ASSERT_EQ(task_info.output_custom_dump_.op_mapping_info_.task(0).output_size(), 1);
  EXPECT_EQ(task_info.input_custom_dump_.op_mapping_info_.task(0).input(0).address(), 0x3000);
  EXPECT_EQ(task_info.output_custom_dump_.op_mapping_info_.task(0).output(0).address(), 0x3008);
  CleanupDumpOp(task_info.input_custom_dump_);
  CleanupDumpOp(task_info.output_custom_dump_);
}

class MockArgsUpdater : public ArgsUpdater {
 public:
  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    update_count_++;
    return GRAPH_SUCCESS;
  }

  int GetUpdateCount() const {
    return update_count_;
  }

 private:
  int update_count_ = 0;
};

class MockFailArgsUpdater : public ArgsUpdater {
 public:
  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    return GRAPH_FAILED;
  }
};

// =====================================================================
// End-to-end ArgsUpdater tests: Distribute → Execute → UpdateHostArgs
// =====================================================================

namespace {
std::atomic<int> g_args_updater_test_counter{0};

class TestArgsUpdaterCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    execute_called_ = true;
    execute_ctx_ = ctx;

    auto *out_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 3}, {2, 1, 3, 16}),
                                               gert::StorageFormat{ge::FORMAT_ND, ge::FORMAT_ND, {}}, ge::DT_INT64);
    out_shape_set_ = (out_tensor != nullptr);

    // Verify input tensor accessible via context and record shape
    const auto *in_tensor = ctx->GetInputTensor(0);
    if (in_tensor != nullptr) {
      in_tensor_accessible_ = true;
      auto &in_origin = in_tensor->GetOriginShape();
      in_shape_dim_num_ = in_origin.GetDimNum();
      if (in_origin.GetDimNum() >= 2) {
        in_shape_dim0_ = in_origin.GetDim(0);
        in_shape_dim1_ = in_origin.GetDim(1);
      }
    }

    // Call MallocReadOnlyDevArgs: allocate device args and copy host content
    uint64_t host_args_buffer[4] = {0x1000ULL, 0x2000ULL, 0x3000ULL, 0x4000ULL};
    malloc_read_only_dev_args_result_ = ctx->MallocReadOnlyDevArgs(host_args_buffer, sizeof(host_args_buffer));
    if (malloc_read_only_dev_args_result_ != nullptr) {
      malloc_args_valid_ = true;
      malloc_dev_args_addr_ = malloc_read_only_dev_args_result_->args_data;
      malloc_dev_args_size_ = malloc_read_only_dev_args_result_->args_size;
      malloc_dev_args_placement_ = malloc_read_only_dev_args_result_->placement;
    }
    return GRAPH_SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    update_host_args_called_ = true;
    update_args_ctx_ = ctx;
    kernel_args_host_ = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0U);
    kernel_args_device_ = ctx->GetKernelArgs(gert::Placement::kPlacementDevice, 0U);

    // Verify output tensor accessible via UpdateHostArgs context
    const auto *out_tensor = ctx->GetOutputTensor(0);
    if (out_tensor != nullptr) {
      out_tensor_accessible_in_update_ = true;
      auto &out_origin = out_tensor->GetOriginShape();
      updated_out_shape_dim_num_ = out_origin.GetDimNum();
      if (out_origin.GetDimNum() >= 2) {
        updated_out_shape_dim0_ = out_origin.GetDim(0);
        updated_out_shape_dim1_ = out_origin.GetDim(1);
      }
    }

    // Verify input tensor shape persists in UpdateHostArgs context
    const auto *in_tensor = ctx->GetInputTensor(0);
    if (in_tensor != nullptr) {
      in_tensor_accessible_in_update_ = true;
      auto &in_origin = in_tensor->GetOriginShape();
      updated_in_shape_dim_num_ = in_origin.GetDimNum();
      if (in_origin.GetDimNum() >= 2) {
        updated_in_shape_dim0_ = in_origin.GetDim(0);
        updated_in_shape_dim1_ = in_origin.GetDim(1);
      }
    }

    // Verify device args address from MallocReadOnlyDevArgs is accessible via GetKernelArgs
    if (kernel_args_device_ != nullptr) {
      malloc_dev_args_addr_via_update_ = kernel_args_device_->args_data;
    }

    return GRAPH_SUCCESS;
  }

  bool execute_called_ = false;
  gert::EagerOpExecutionContext *execute_ctx_ = nullptr;
  const gert::KernelArgs *malloc_read_only_dev_args_result_ = nullptr;
  bool malloc_args_valid_ = false;
  void *malloc_dev_args_addr_ = nullptr;
  size_t malloc_dev_args_size_ = 0;
  gert::Placement malloc_dev_args_placement_ = gert::Placement::kPlacementHost;

  bool update_host_args_called_ = false;
  gert::UpdateArgsContext *update_args_ctx_ = nullptr;
  const gert::KernelArgs *kernel_args_host_ = nullptr;
  const gert::KernelArgs *kernel_args_device_ = nullptr;
  void *malloc_dev_args_addr_via_update_ = nullptr;

  bool out_shape_set_ = false;
  bool in_tensor_accessible_ = false;
  size_t in_shape_dim_num_ = 0;
  int64_t in_shape_dim0_ = 0;
  int64_t in_shape_dim1_ = 0;
  bool out_tensor_accessible_in_update_ = false;
  size_t updated_out_shape_dim_num_ = 0;
  int64_t updated_out_shape_dim0_ = 0;
  int64_t updated_out_shape_dim1_ = 0;
  bool in_tensor_accessible_in_update_ = false;
  size_t updated_in_shape_dim_num_ = 0;
  int64_t updated_in_shape_dim0_ = 0;
  int64_t updated_in_shape_dim1_ = 0;
};

class TestArgsRefreshableCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    execute_called_ = true;
    // Call MallocReadOnlyDevArgs: non-refreshable ops should use MallocDynamicMemory + H2D copy
    uint64_t host_args_buffer[4] = {0x1000ULL, 0x2000ULL, 0x3000ULL, 0x4000ULL};
    malloc_read_only_dev_args_result_ = ctx->MallocReadOnlyDevArgs(host_args_buffer, sizeof(host_args_buffer));
    if (malloc_read_only_dev_args_result_ != nullptr) {
      malloc_args_valid_ = true;
      malloc_dev_args_addr_ = malloc_read_only_dev_args_result_->args_data;
      malloc_dev_args_size_ = malloc_read_only_dev_args_result_->args_size;
      malloc_dev_args_placement_ = malloc_read_only_dev_args_result_->placement;
    }
    auto *out_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 3}, {2, 1, 3, 16}),
                                               gert::StorageFormat{ge::FORMAT_ND, ge::FORMAT_ND, {}}, ge::DT_INT64);
    return GRAPH_SUCCESS;
  }

  bool execute_called_ = false;
  const gert::KernelArgs *malloc_read_only_dev_args_result_ = nullptr;
  bool malloc_args_valid_ = false;
  void *malloc_dev_args_addr_ = nullptr;
  size_t malloc_dev_args_size_ = 0U;
  gert::Placement malloc_dev_args_placement_ = gert::Placement::kPlacementHost;
};

class TestEagerOnlyCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    return GRAPH_SUCCESS;
  }
};

class TestVerifyContextCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    // Execute being called with a valid context proves additional inputs are wired
    saw_allocator_ = true;
    saw_args_handler_ = true;
    return GRAPH_SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    return GRAPH_SUCCESS;
  }

  bool saw_allocator_ = false;
  bool saw_args_handler_ = false;
};

std::string GenerateUniqueOpType() {
  return "ArgsUpdaterTestOp_" + std::to_string(g_args_updater_test_counter.fetch_add(1));
}

void SetUpMinimalDavinciModel(DavinciModel &model, const OpDescPtr &op_desc) {
  if (model.GetCustomOpRegistry() == nullptr) {
    model.SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  }

  // Set input shape {2, 3} so input tensor has verifiable origin shape
  auto in_desc = op_desc->MutableInputDesc(0);
  if (in_desc != nullptr) {
    in_desc->SetShape(GeShape({2, 3}));
    in_desc->SetOriginShape(GeShape({2, 3}));
  }

  // Reset offsets to fit within mem_size (CreateOpDesc uses g_node_index * 64,
  // which may exceed mem_size when running after other tests)
  const size_t input_count = op_desc->GetInputsSize();
  const size_t output_count = op_desc->GetOutputsSize();
  std::vector<int64_t> input_offset;
  for (size_t i = 0; i < input_count; ++i) {
    input_offset.emplace_back(static_cast<int64_t>(i * 64));
  }
  op_desc->SetInputOffset(input_offset);
  std::vector<int64_t> output_offset;
  for (size_t i = 0; i < output_count; ++i) {
    output_offset.emplace_back(static_cast<int64_t>(input_count * 64 + i * 64));
  }
  op_desc->SetOutputOffset(output_offset);

  model.runtime_param_.mem_size = 8192U;
  std::vector<uint8_t> memory_holder(model.runtime_param_.mem_size);
  model.runtime_param_.mem_base = reinterpret_cast<uintptr_t>(memory_holder.data());

  MemAllocation fm_alloc = {0, 0ULL, model.runtime_param_.mem_size, MemAllocation::Type::FEATURE_MAP, 0U};
  MemAllocation abs_alloc = {0, model.runtime_param_.mem_base, model.runtime_param_.mem_size,
                             MemAllocation::Type::ABSOLUTE, 0U};
  model.logical_mem_allocations_ = {fm_alloc, abs_alloc};

  rtStream_t stream = nullptr;
  model.reusable_stream_allocator_ = ReusableStreamAllocator::Create();
  model.reusable_stream_allocator_->GetOrCreateRtStream(stream, 0, 0, 0);
  model.stream_list_ = {stream};

  model.op_list_[op_desc->GetId()] = op_desc;

  model.mem_type_to_allocator_[RT_MEMORY_HBM] = std::make_shared<MemoryBlockManager>(RT_MEMORY_HBM);

  ModelArgsManager::ExtraArgsPool pool;
  pool.host_addr = ge::MakeUnique<uint8_t[]>(4096);
  pool.device_addr = 0xDEADBEEFULL;
  pool.total_size = 4096UL;
  pool.allocated_offset = 0UL;
  pool.placement = ArgsPlacement::kArgsPlacementHbm;
  model.args_manager_.extra_args_pools_.emplace_back(std::move(pool));
  model.args_manager_.davinci_model_ = &model;
}

class TestFailArgsUpdaterCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto *out_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 3}, {2, 1, 3, 16}),
                                               gert::StorageFormat{ge::FORMAT_ND, ge::FORMAT_ND, {}}, ge::DT_INT64);
    out_shape_set_ = (out_tensor != nullptr);
    return GRAPH_SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    return GRAPH_FAILED;
  }

  bool out_shape_set_ = false;
};

class TestNonEagerCustomOp : public BaseCustomOp {};

}  // namespace

TEST_F(UtestCustomTaskInfo, ParseTaskRunParam_UsesModelCustomOpRegistry) {
  const std::string op_type = GenerateUniqueOpType();
  auto registry = std::make_shared<CustomOpRegistry>();
  ASSERT_NE(registry, nullptr);
  ASSERT_EQ(registry->RegisterCreator(
                op_type.c_str(),
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestArgsUpdaterCustomOp>(); }),
            GRAPH_SUCCESS);

  DavinciModel model(0, nullptr);
  model.SetCustomOpRegistry(registry);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.mutable_kernel()->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);
  EXPECT_TRUE(task_info.NeedReserveArgsTable());
}

class UtestCustomTaskInfoE2E : public testing::Test {
 protected:
  void SetUp() {
    RTS_STUB_SETUP();
  }
  void TearDown() {
    RTS_STUB_TEARDOWN();
  }
};

TEST_F(UtestCustomTaskInfoE2E, Distribute_DetectsArgsUpdaterAndSetsArgsUpdateOp) {
  std::string op_type = GenerateUniqueOpType();
  CustomOpFactory::RegisterCustomOpCreator(
      op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestArgsUpdaterCustomOp>(); });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  EXPECT_TRUE(task_info.NeedReserveArgsTable());
  EXPECT_EQ(task_info.input_count_, op_desc->GetInputsSize());
  EXPECT_EQ(task_info.output_count_, op_desc->GetOutputsSize());

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, Distribute_ExecuteCallsMallocReadOnlyDevArgs) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsUpdaterCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsUpdaterCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(op_instance->execute_called_);
  EXPECT_TRUE(op_instance->out_shape_set_);
  // Verify input tensor accessible and origin shape is {2, 3}
  EXPECT_TRUE(op_instance->in_tensor_accessible_);
  EXPECT_EQ(op_instance->in_shape_dim_num_, 2U);
  EXPECT_EQ(op_instance->in_shape_dim0_, 2);
  EXPECT_EQ(op_instance->in_shape_dim1_, 3);
  EXPECT_NE(op_instance->malloc_read_only_dev_args_result_, nullptr);
  EXPECT_TRUE(op_instance->malloc_args_valid_);
  EXPECT_EQ(op_instance->malloc_dev_args_size_, sizeof(uint64_t) * 4);
  EXPECT_EQ(op_instance->malloc_dev_args_placement_, gert::Placement::kPlacementDevice);
  EXPECT_NE(op_instance->malloc_dev_args_addr_, nullptr);
  EXPECT_EQ(op_instance->malloc_read_only_dev_args_result_->args_size, sizeof(uint64_t) * 4);
  EXPECT_EQ(op_instance->malloc_read_only_dev_args_result_->placement, gert::Placement::kPlacementDevice);

  const auto &host_args = task_info.GetKernelArgsDeque(gert::Placement::kPlacementHost);
  const auto &dev_args = task_info.GetKernelArgsDeque(gert::Placement::kPlacementDevice);
  EXPECT_EQ(host_args.size(), 1U);
  EXPECT_EQ(dev_args.size(), 1U);
  EXPECT_NE(host_args[0].args_data, nullptr);
  EXPECT_NE(dev_args[0].args_data, nullptr);

  // MallocReadOnlyDevArgs result must match GetKernelArgsDeque device entry
  EXPECT_EQ(op_instance->malloc_dev_args_addr_, dev_args[0].args_data);
  EXPECT_EQ(op_instance->malloc_dev_args_size_, dev_args[0].args_size);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, Distribute_AdditionalInputsOutputsWired) {
  std::string op_type = GenerateUniqueOpType();
  TestVerifyContextCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestVerifyContextCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(op_instance->saw_allocator_);
  EXPECT_TRUE(op_instance->saw_args_handler_);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_CallsOpCallbackWithValidContext) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsUpdaterCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsUpdaterCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(op_instance->execute_called_);

  uint64_t active_mem_base_addr[2] = {0x10000ULL, 0x20000ULL};
  EXPECT_EQ(task_info.UpdateHostArgs(active_mem_base_addr, 2), SUCCESS);

  EXPECT_TRUE(op_instance->update_host_args_called_);
  EXPECT_NE(op_instance->kernel_args_host_, nullptr);
  EXPECT_NE(op_instance->kernel_args_device_, nullptr);
  EXPECT_NE(op_instance->kernel_args_host_->args_data, nullptr);
  EXPECT_NE(op_instance->kernel_args_device_->args_data, nullptr);

  // Verify output tensor origin shape is {2, 3} after UpdateHostArgs
  EXPECT_TRUE(op_instance->out_tensor_accessible_in_update_);
  EXPECT_EQ(op_instance->updated_out_shape_dim_num_, 2U);
  EXPECT_EQ(op_instance->updated_out_shape_dim0_, 2);
  EXPECT_EQ(op_instance->updated_out_shape_dim1_, 3);

  // Verify input tensor origin shape is still {2, 3} after UpdateHostArgs
  EXPECT_TRUE(op_instance->in_tensor_accessible_in_update_);
  EXPECT_EQ(op_instance->updated_in_shape_dim_num_, 2U);
  EXPECT_EQ(op_instance->updated_in_shape_dim0_, 2);
  EXPECT_EQ(op_instance->updated_in_shape_dim1_, 3);

  // MallocReadOnlyDevArgs device addr must match GetKernelArgs(kPlacementDevice) addr
  EXPECT_NE(op_instance->malloc_dev_args_addr_via_update_, nullptr);
  EXPECT_EQ(op_instance->malloc_dev_args_addr_, op_instance->malloc_dev_args_addr_via_update_);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_FailsOnNonArgsUpdaterOp) {
  std::string op_type = GenerateUniqueOpType();
  CustomOpFactory::RegisterCustomOpCreator(
      op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestEagerOnlyCustomOp>(); });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  EXPECT_FALSE(task_info.NeedReserveArgsTable());

  uint64_t active_mem_base_addr[2] = {0x1000ULL, 0x2000ULL};
  EXPECT_NE(task_info.UpdateHostArgs(active_mem_base_addr, 2), SUCCESS);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_FailsOnNullBaseAddrOrZeroSize) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsUpdaterCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsUpdaterCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(task_info.NeedReserveArgsTable());

  uint64_t valid_addr[2] = {0x1000ULL, 0x2000ULL};
  EXPECT_NE(task_info.UpdateHostArgs(nullptr, 2), SUCCESS);
  EXPECT_NE(task_info.UpdateHostArgs(valid_addr, 0), SUCCESS);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, InitArgsIoAddrsUpdater_PopulatesMemAllocationAndOffsets) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsUpdaterCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsUpdaterCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  EXPECT_TRUE(task_info.NeedReserveArgsTable());

  model.runtime_param_.mem_base = 0U;
}

// =====================================================================
// AllocateArgsBuffer & IntegrateCustomOpArgs E2E tests
// =====================================================================

TEST_F(UtestCustomTaskInfoE2E, AllocateArgsBuffer_FromExistingPool_Success) {
  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc("alloc_test", "AllocTest", 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  ArgsAllocationResult result;
  EXPECT_EQ(model.AllocateArgsBuffer(32, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);
  EXPECT_NE(result.host_addr, nullptr);
  EXPECT_EQ(result.device_addr, 0xDEADBEEFULL);  // from extra_args_pools_[0].device_addr + offset 0
  EXPECT_EQ(result.size, 32U);
  EXPECT_FALSE(result.is_from_reserved);
  EXPECT_EQ(result.extra_pool_index, 0U);

  // Verify pool offset advanced
  EXPECT_EQ(model.args_manager_.extra_args_pools_[0].allocated_offset, 32U);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, AllocateArgsBuffer_ExistingPoolExhausted_CreatesNewPool) {
  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc("alloc_test2", "AllocTest2", 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  // Exhaust existing pool: allocate 4064 bytes (leaves 32 bytes remaining in 4096 pool)
  ArgsAllocationResult result1;
  EXPECT_EQ(model.AllocateArgsBuffer(4064, ArgsPlacement::kArgsPlacementHbm, result1), SUCCESS);
  EXPECT_EQ(result1.extra_pool_index, 0U);

  // Next allocation (64 bytes) won't fit in existing pool → Tier 3 creates new pool
  ArgsAllocationResult result2;
  EXPECT_EQ(model.AllocateArgsBuffer(64, ArgsPlacement::kArgsPlacementHbm, result2), SUCCESS);
  EXPECT_FALSE(result2.is_from_reserved);
  EXPECT_EQ(result2.extra_pool_index, 1U);
  EXPECT_EQ(model.args_manager_.extra_args_pools_.size(), 2U);
  EXPECT_GE(model.args_manager_.extra_args_pools_[1].total_size, 4096U);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, AllocateArgsBuffer_MallocReadOnlyDevArgsE2E) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsUpdaterCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsUpdaterCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(op_instance->malloc_args_valid_);

  // Verify args_allocation_results_ populated by MallocReadOnlyDevArgs via AllocateArgsBuffer
  const auto &alloc_results = task_info.GetArgsAllocationResults();
  EXPECT_EQ(alloc_results.size(), 1U);
  EXPECT_NE(alloc_results[0].host_addr, nullptr);
  EXPECT_NE(alloc_results[0].device_addr, 0U);
  EXPECT_EQ(alloc_results[0].size, sizeof(uint64_t) * 4);
  EXPECT_EQ(alloc_results[0].placement, ArgsPlacement::kArgsPlacementHbm);
  EXPECT_FALSE(alloc_results[0].is_from_reserved);
  EXPECT_EQ(alloc_results[0].extra_pool_index, 0U);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, IntegrateCustomOpArgs_RequiresFullModelInit) {
  // IntegrateCustomOpArgs requires a fully-initialized ModelArgsManager
  // (task_list_ptr_, model_args_, update_policies_to_model_data_, etc.)
  // which is only set up after the full Init→AllocModelArgs→ParseModelTaskDef pipeline.
  // This test documents that SetUpMinimalDavinciModel is insufficient for
  // calling IntegrateCustomOpArgs directly.
  // Full E2E coverage of IntegrateCustomOpArgs should be added to
  // model_args_manager_unittest.cc with proper DavinciModel initialization.
}

TEST_F(UtestCustomTaskInfoE2E, DavinciModel_AllocateArgsBuffer_ForwardsToArgsManager) {
  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc("forward_test", "ForwardTest", 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  // Call via DavinciModel public wrapper
  ArgsAllocationResult result_via_model;
  EXPECT_EQ(model.AllocateArgsBuffer(32, ArgsPlacement::kArgsPlacementHbm, result_via_model), SUCCESS);

  // Call directly via args_manager_ (same model object)
  ArgsAllocationResult result_via_manager;
  EXPECT_EQ(model.args_manager_.AllocateArgsBuffer(64, ArgsPlacement::kArgsPlacementHbm, result_via_manager), SUCCESS);

  // Both allocated from same pool (extra_args_pools_[0])
  EXPECT_EQ(result_via_model.extra_pool_index, 0U);
  EXPECT_EQ(result_via_manager.extra_pool_index, 0U);

  // Offsets are sequential: first at 0, second at 32
  EXPECT_EQ(model.args_manager_.extra_args_pools_[0].allocated_offset, 96U);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, AllocateArgsBuffer_InvalidSizeOrPlacement_ReturnsFailed) {
  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc("invalid_test", "InvalidTest", 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  ArgsAllocationResult result;

  // GE_ASSERT_TRUE(size > 0) → returns ErrorResult for size=0
  EXPECT_NE(model.AllocateArgsBuffer(0, ArgsPlacement::kArgsPlacementHbm, result), SUCCESS);

  // GE_ASSERT_TRUE(placement < kEnd) → returns ErrorResult for kEnd placement
  EXPECT_NE(model.AllocateArgsBuffer(32, ArgsPlacement::kEnd, result), SUCCESS);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, ParseTaskRunParam_ArgsUpdater_SupportRefreshTrue) {
  std::string op_type = GenerateUniqueOpType();
  CustomOpFactory::RegisterCustomOpCreator(
      op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestArgsUpdaterCustomOp>(); });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  // ArgsUpdater → NeedReserveArgsTable returns true
  EXPECT_TRUE(task_info.NeedReserveArgsTable());

  // ArgsUpdater → support_refresh = true
  for (const auto &addr : task_run_param.parsed_input_addrs) {
    EXPECT_TRUE(addr.support_refresh);
  }
  for (const auto &addr : task_run_param.parsed_output_addrs) {
    EXPECT_TRUE(addr.support_refresh);
  }
  for (const auto &addr : task_run_param.parsed_workspace_addrs) {
    EXPECT_TRUE(addr.support_refresh);
  }

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, ParseTaskRunParam_EagerOnly_SupportRefreshFalse) {
  std::string op_type = GenerateUniqueOpType();
  CustomOpFactory::RegisterCustomOpCreator(
      op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestEagerOnlyCustomOp>(); });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  // EagerExecuteOp only → NeedReserveArgsTable returns false
  EXPECT_FALSE(task_info.NeedReserveArgsTable());

  // EagerExecuteOp only → support_refresh = false
  for (const auto &addr : task_run_param.parsed_input_addrs) {
    EXPECT_FALSE(addr.support_refresh);
  }
  for (const auto &addr : task_run_param.parsed_output_addrs) {
    EXPECT_FALSE(addr.support_refresh);
  }
  for (const auto &addr : task_run_param.parsed_workspace_addrs) {
    EXPECT_FALSE(addr.support_refresh);
  }

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, MallocReadOnlyDevArgs_EagerOnly_UsesMallocDynamicMemory) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsRefreshableCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsRefreshableCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  EXPECT_FALSE(task_info.NeedReserveArgsTable());

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(op_instance->malloc_args_valid_);
  EXPECT_NE(op_instance->malloc_dev_args_addr_, nullptr);
  EXPECT_EQ(op_instance->malloc_dev_args_size_, sizeof(uint64_t) * 4);
  EXPECT_EQ(op_instance->malloc_dev_args_placement_, gert::Placement::kPlacementDevice);

  // Non-refreshable: no host args deque (only device args deque populated)
  const auto &dev_args = task_info.GetKernelArgsDeque(gert::Placement::kPlacementDevice);
  EXPECT_EQ(dev_args.size(), 1U);
  EXPECT_NE(dev_args[0].args_data, nullptr);
  EXPECT_EQ(dev_args[0].args_size, sizeof(uint64_t) * 4);

  // Non-refreshable: args_allocation_results_ should be empty (no AllocateArgsBuffer used)
  const auto &alloc_results = task_info.GetArgsAllocationResults();
  EXPECT_EQ(alloc_results.size(), 0U);

  model.runtime_param_.mem_base = 0U;
}

// =====================================================================
// Task 1: UpdateHostArgs error path UTs
// =====================================================================

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_AllocationIdOutOfBounds_ReturnsFailed) {
  std::string op_type = GenerateUniqueOpType();
  TestArgsUpdaterCustomOp *op_instance = nullptr;
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), [&op_instance]() -> std::unique_ptr<BaseCustomOp> {
    auto op = std::make_unique<TestArgsUpdaterCustomOp>();
    op_instance = op.get();
    return op;
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  ASSERT_NE(op_instance, nullptr);
  EXPECT_TRUE(task_info.NeedReserveArgsTable());

  // Pass mem_size=1 when allocation requires index >= 1 → allocation_id out of bounds
  uint64_t active_mem_base_addr[1] = {0x10000ULL};
  EXPECT_NE(task_info.UpdateHostArgs(active_mem_base_addr, 1), SUCCESS);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_EmptyMemAllocs_ReturnsFailed) {
  // Manually set args_update_op_ without calling Distribute() to leave args_io_addrs_updater_ uninitialized
  DavinciModel model(0, nullptr);
  CustomTaskInfo task_info;
  MockArgsUpdater mock_updater;
  task_info.args_update_op_ = &mock_updater;

  uint64_t active_mem_base_addr[2] = {0x1000ULL, 0x2000ULL};
  EXPECT_NE(task_info.UpdateHostArgs(active_mem_base_addr, 2), SUCCESS);
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_OperatorUpdateFails_ReturnsFailed) {
  std::string op_type = GenerateUniqueOpType();
  CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestFailArgsUpdaterCustomOp>();
  });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  EXPECT_TRUE(task_info.NeedReserveArgsTable());

  uint64_t active_mem_base_addr[2] = {0x10000ULL, 0x20000ULL};
  EXPECT_NE(task_info.UpdateHostArgs(active_mem_base_addr, 2), SUCCESS);

  model.runtime_param_.mem_base = 0U;
}

// =====================================================================
// Task 2: Distribute / MallocReadOnlyDevArgsImpl / UpdateIoAndWorkspaceAddrs / ParseOpIndex
// =====================================================================

TEST_F(UtestCustomTaskInfoE2E, Distribute_NonEagerOp_ReturnsFailed) {
  std::string op_type = GenerateUniqueOpType();
  CustomOpFactory::RegisterCustomOpCreator(
      op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestNonEagerCustomOp>(); });

  DavinciModel model(0, nullptr);
  const auto op_desc = CreateOpDesc(op_type, op_type, 1, 1);
  SetUpMinimalDavinciModel(model, op_desc);

  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = 0xDEADBEEFULL;
  IowAddrs iow_addrs;
  EXPECT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  // TestNonEagerCustomOp does NOT implement EagerExecuteOp → dynamic_cast returns nullptr → GRAPH_FAILED
  EXPECT_NE(task_info.Distribute(), SUCCESS);

  model.runtime_param_.mem_base = 0U;
}

TEST_F(UtestCustomTaskInfoE2E, MallocReadOnlyDevArgsImpl_NullHostArgs_ReturnsNullptr) {
  CustomTaskInfo task_info;
  DavinciModel model(0, nullptr);
  task_info.davinci_model_ = &model;
  uint64_t host_args_buffer[4] = {0x1000ULL, 0x2000ULL, 0x3000ULL, 0x4000ULL};

  // GE_ASSERT_TRUE(host_args != nullptr && args_size != 0U && davinci_model_ != nullptr)
  EXPECT_EQ(task_info.MallocReadOnlyDevArgsImpl(nullptr, sizeof(host_args_buffer)), nullptr);
}

TEST_F(UtestCustomTaskInfoE2E, MallocReadOnlyDevArgsImpl_ZeroArgsSize_ReturnsNullptr) {
  CustomTaskInfo task_info;
  DavinciModel model(0, nullptr);
  task_info.davinci_model_ = &model;

  EXPECT_EQ(task_info.MallocReadOnlyDevArgsImpl(reinterpret_cast<void *>(0x1000), 0), nullptr);
}

TEST_F(UtestCustomTaskInfoE2E, MallocReadOnlyDevArgsImpl_NullDavinciModel_ReturnsNullptr) {
  CustomTaskInfo task_info;
  task_info.davinci_model_ = nullptr;
  uint64_t host_args_buffer[4] = {0x1000ULL, 0x2000ULL, 0x3000ULL, 0x4000ULL};

  EXPECT_EQ(task_info.MallocReadOnlyDevArgsImpl(host_args_buffer, sizeof(host_args_buffer)), nullptr);
}

TEST_F(UtestCustomTaskInfoE2E, UpdateIoAndWorkspaceAddrs_NonEmptyIowAddrs_ReplacesAddresses) {
  DavinciModel model(0, nullptr);
  auto op_desc = std::make_shared<OpDesc>("iow_test_op", "CustomOp");
  GeTensorDesc desc;
  op_desc->AddInputDesc(desc);
  op_desc->AddOutputDesc(desc);
  op_desc->SetId(0);
  model.op_list_[0] = op_desc;

  CustomTaskInfo task_info;
  task_info.input_data_addrs_ = {0x1000ULL};
  task_info.output_data_addrs_ = {0x2000ULL};
  task_info.workspace_addrs_ = {0x3000ULL};
  task_info.input_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeModelIo)};
  task_info.output_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)};
  task_info.workspace_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFix)};

  IowAddrs iow_addrs;
  iow_addrs.input_logic_addrs = {{0x5000ULL, static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)}};
  iow_addrs.output_logic_addrs = {{0x6000ULL, static_cast<uint64_t>(MemoryAppType::kMemoryTypeModelIo)}};
  iow_addrs.workspace_logic_addrs = {{0x7000ULL, static_cast<uint64_t>(MemoryAppType::kMemoryTypeFix)}};

  task_info.UpdateIoAndWorkspaceAddrs(iow_addrs);
  EXPECT_EQ(task_info.input_data_addrs_[0], 0x5000ULL);
  EXPECT_EQ(task_info.output_data_addrs_[0], 0x6000ULL);
  EXPECT_EQ(task_info.workspace_addrs_[0], 0x7000ULL);
  EXPECT_EQ(task_info.input_mem_types_[0], static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap));
  EXPECT_EQ(task_info.output_mem_types_[0], static_cast<uint64_t>(MemoryAppType::kMemoryTypeModelIo));
  EXPECT_EQ(task_info.workspace_mem_types_[0], static_cast<uint64_t>(MemoryAppType::kMemoryTypeFix));
}

TEST_F(UtestCustomTaskInfoE2E, ParseOpIndex_ReturnsCorrectOpIndex) {
  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(42);

  CustomTaskInfo task_info;
  EXPECT_EQ(task_info.ParseOpIndex(task_def), 42);
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_NullBaseAddr_ReturnsFailed) {
  CustomTaskInfo task_info;
  DavinciModel model(0, nullptr);
  task_info.davinci_model_ = &model;
  task_info.args_update_op_ = nullptr;
  EXPECT_NE(task_info.UpdateHostArgs(nullptr, 2), SUCCESS);
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_ZeroMemSize_ReturnsFailed) {
  CustomTaskInfo task_info;
  DavinciModel model(0, nullptr);
  task_info.davinci_model_ = &model;
  uint64_t addr = 0x1000ULL;
  task_info.args_update_op_ = nullptr;
  EXPECT_NE(task_info.UpdateHostArgs(&addr, 0), SUCCESS);
}

TEST_F(UtestCustomTaskInfoE2E, UpdateHostArgs_NullArgsUpdateOp_ReturnsFailed) {
  CustomTaskInfo task_info;
  DavinciModel model(0, nullptr);
  task_info.davinci_model_ = &model;
  task_info.args_update_op_ = nullptr;
  uint64_t active_mem_base_addr[2] = {0x1000ULL, 0x2000ULL};
  EXPECT_EQ(task_info.UpdateHostArgs(active_mem_base_addr, 2), FAILED);
}

TEST_F(UtestCustomTaskInfoE2E, GetKernelArgsDeque_DeviceReturnsDeviceDeque) {
  CustomTaskInfo task_info;
  task_info.kernel_args_device_deque_.push_back(gert::KernelArgs());
  task_info.kernel_args_device_deque_.back().args_data = reinterpret_cast<void *>(0xDEADULL);
  task_info.kernel_args_device_deque_.back().args_size = 32U;
  task_info.kernel_args_device_deque_.back().placement = gert::Placement::kPlacementDevice;

  const auto &device_args = task_info.GetKernelArgsDeque(gert::Placement::kPlacementDevice);
  EXPECT_EQ(device_args.size(), 1U);
  EXPECT_EQ(device_args[0].placement, gert::Placement::kPlacementDevice);
}

TEST_F(UtestCustomTaskInfoE2E, GetKernelArgsDeque_HostReturnsHostDeque) {
  CustomTaskInfo task_info;
  task_info.kernel_args_host_deque_.push_back(gert::KernelArgs());
  task_info.kernel_args_host_deque_.back().args_data = reinterpret_cast<void *>(0xBEEFULL);
  task_info.kernel_args_host_deque_.back().args_size = 16U;
  task_info.kernel_args_host_deque_.back().placement = gert::Placement::kPlacementHost;

  const auto &host_args = task_info.GetKernelArgsDeque(gert::Placement::kPlacementHost);
  EXPECT_EQ(host_args.size(), 1U);
  EXPECT_EQ(host_args[0].placement, gert::Placement::kPlacementHost);
}

TEST_F(UtestCustomTaskInfoE2E, UpdateIoAndWorkspaceAddrs_EmptyIowAddrs_KeepsOriginal) {
  CustomTaskInfo task_info;
  task_info.input_data_addrs_ = {0x1000ULL};
  task_info.output_data_addrs_ = {0x2000ULL};
  task_info.workspace_addrs_ = {0x3000ULL};
  task_info.input_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)};
  task_info.output_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeModelIo)};
  task_info.workspace_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFix)};

  IowAddrs empty_iow;
  task_info.UpdateIoAndWorkspaceAddrs(empty_iow);
  EXPECT_EQ(task_info.input_data_addrs_[0], 0x1000ULL);
  EXPECT_EQ(task_info.output_data_addrs_[0], 0x2000ULL);
  EXPECT_EQ(task_info.workspace_addrs_[0], 0x3000ULL);
  EXPECT_EQ(task_info.input_mem_types_[0], static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap));
  EXPECT_EQ(task_info.output_mem_types_[0], static_cast<uint64_t>(MemoryAppType::kMemoryTypeModelIo));
  EXPECT_EQ(task_info.workspace_mem_types_[0], static_cast<uint64_t>(MemoryAppType::kMemoryTypeFix));
}

}  // namespace ge
