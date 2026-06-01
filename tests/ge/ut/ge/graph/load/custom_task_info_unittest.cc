/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/load/model_manager/task_info/ge/custom_task_info.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "depends/ascendcl/src/ascendcl_stub.h"
#include "depends/runtime/src/runtime_stub.h"
#include "graph/load/model_manager/davinci_model.h"

namespace ge {
namespace {
class AclMockMemcpy : public AclRuntimeStub {
 public:
  MOCK_METHOD5(aclrtMemcpy, int32_t(void *, size_t, const void *, size_t, aclrtMemcpyKind)
  );
};
}

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
}  // namespace ge
