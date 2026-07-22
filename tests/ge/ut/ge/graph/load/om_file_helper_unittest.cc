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
#include "macro_utils/dt_public_scope.h"
#include "framework/common/helper/om_file_helper.h"
#include "common/helper/file_saver.h"
#include "common/math/ge_math_util.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/debug/log.h"
#include "framework/common/ge_inner_error_codes.h"
#include "framework/common/util.h"
#include "graph/def_types.h"
#include "graph/utils/attr_utils.h"
#include "macro_utils/dt_public_unscope.h"

#include "depends/ascendcl/src/ascendcl_stub.h"
#include "proto/task.pb.h"

using namespace std;

namespace ge {
class UtestOmFileHelper : public testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UtestOmFileHelper, Normal) {
  EXPECT_NO_THROW(OmFileLoadHelper loader);
  EXPECT_NO_THROW(OmFileSaveHelper saver);
}

TEST_F(UtestOmFileHelper, LoadInit) {
  OmFileLoadHelper loader;
  ModelData md;
  EXPECT_EQ(loader.Init(md), PARAM_INVALID);
  md.model_len = sizeof(ModelFileHeader) + sizeof(ModelPartitionTable) + 10;
  uint8_t *data = new uint8_t[md.model_len];
  md.model_data = data;
  ModelFileHeader *header = reinterpret_cast<ModelFileHeader *>(data);
  header->model_length = md.model_len - sizeof(ModelFileHeader);
  header->magic = MODEL_FILE_MAGIC_NUM;
  header->modeltype = MODEL_TYPE_FLOW_MODEL;
  ModelPartitionTable *partition_table = reinterpret_cast<ModelPartitionTable *>(data + sizeof(ModelFileHeader));
  partition_table->num = 5000;
  EXPECT_EQ(loader.Init(md), ACL_ERROR_GE_PARAM_INVALID);
  delete[] data;
}

TEST_F(UtestOmFileHelper, GetModelPartition) {
  OmFileLoadHelper loader;
  loader.is_inited_ = true;
  ModelPartitionType type = ModelPartitionType::MODEL_DEF;
  ModelPartition partition;
  size_t model_index = 10;
  EXPECT_EQ(loader.GetModelPartition(type, partition, model_index), PARAM_INVALID);
  loader.is_inited_ = false;
  EXPECT_EQ(loader.GetModelPartition(type, partition, model_index), PARAM_INVALID);
  loader.is_inited_ = true;
  model_index = 0;
  type = TASK_INFO;
  ASSERT_TRUE(loader.model_contexts_.empty());
  EXPECT_EQ(loader.GetModelPartition(type, partition, model_index), PARAM_INVALID);
  ASSERT_TRUE(loader.model_contexts_.empty());

  loader.model_contexts_.emplace_back(OmFileContext{});
  EXPECT_EQ(loader.GetModelPartition(CUST_AICPU_KERNELS, partition, model_index), SUCCESS);
}

TEST_F(UtestOmFileHelper, GetModelPartitionNoIndex) {
  OmFileLoadHelper loader;
  ModelPartitionType type = ModelPartitionType::MODEL_DEF;
  ModelPartition partition;
  loader.is_inited_ = false;
  EXPECT_EQ(loader.GetModelPartition(type, partition), PARAM_INVALID);
  loader.is_inited_ = true;
  type = TASK_INFO;
  ASSERT_TRUE(loader.model_contexts_.empty());
  EXPECT_EQ(loader.GetModelPartition(type, partition, 0U), PARAM_INVALID);
  ASSERT_TRUE(loader.model_contexts_.empty());
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTable) {
  OmFileLoadHelper loader;
  uint8_t *model_data = nullptr;
  uint64_t model_data_size = 0;
  size_t mem_offset = 0U;
  EXPECT_EQ(loader.LoadModelPartitionTable(model_data, model_data_size, 0U, mem_offset, nullptr),
            ACL_ERROR_GE_EXEC_MODEL_ADDR_INVALID);
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTableWithNum) {
  OmFileLoadHelper loader;
  uint8_t *model_data = nullptr;
  uint64_t model_data_size = 0;
  uint32_t model_num = 0;
  EXPECT_EQ(loader.LoadModelPartitionTable(model_data, model_data_size, model_num, nullptr), PARAM_INVALID);
}

TEST_F(UtestOmFileHelper, SaverGetPartitionTable1) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  oc.partition_datas_.push_back(ModelPartition());
  saver.model_contexts_.push_back(oc);
  size_t cur_ctx_index = 0;
  EXPECT_NE(saver.GetPartitionTable(cur_ctx_index), nullptr);
}

TEST_F(UtestOmFileHelper, SaverGetPartitionTable2) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  oc.partition_datas_.push_back(ModelPartition());
  saver.model_contexts_.push_back(oc);
  size_t cur_ctx_index = 0;
  const bool is_partition_align = true;
  const uint32_t align_bytes = 8U;  // 8字节对齐
  EXPECT_NE(saver.GetPartitionTable(cur_ctx_index, is_partition_align, align_bytes), nullptr);
}

TEST_F(UtestOmFileHelper, SaveModel) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  oc.model_data_len_ = 1024;
  saver.model_contexts_.push_back(oc);
  const char_t *const output_file = "root.model";
  ModelBufferData model;
  const bool is_offline = true;
  EXPECT_NE(saver.SaveModel(output_file, model, is_offline), SUCCESS);
}

TEST_F(UtestOmFileHelper, SaveModel2) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  oc.model_data_len_ = 1024;
  saver.model_contexts_.push_back(oc);
  const char_t *const output_file = "root.model";
  ModelBufferData model;
  const bool is_offline = true;
  const bool is_partition_align = true;
  const uint32_t align_bytes = 8U;  // 8字节对齐
  EXPECT_NE(saver.SaveModel(output_file, model, is_offline, is_partition_align, align_bytes), SUCCESS);
}

TEST_F(UtestOmFileHelper, AddPartition) {
  OmFileSaveHelper saver;
  ModelPartition partition;
  EXPECT_EQ(saver.AddPartition(partition), SUCCESS);
  ASSERT_FALSE(saver.model_contexts_.empty());
  saver.model_contexts_[0U].model_data_len_ = 4000000000U;
  partition.size = 2000000000U;
  EXPECT_EQ(saver.AddPartition(partition), SUCCESS);
}

TEST_F(UtestOmFileHelper, AddOwnedPartition) {
  OmFileSaveHelper saver;
  std::vector<uint8_t> payload = {0x12U, 0x34U, 0x56U, 0x78U};
  const std::vector<uint8_t> expected = payload;

  EXPECT_EQ(saver.AddOwnedPartition(ModelPartitionType::CUSTOM_OPS, std::move(payload), 0U), SUCCESS);
  ASSERT_EQ(saver.model_contexts_.size(), 1U);

  const auto &ctx = saver.model_contexts_[0U];
  ASSERT_EQ(ctx.partition_datas_.size(), 1U);
  ASSERT_EQ(ctx.owned_partitions_.size(), 1U);
  ASSERT_NE(ctx.owned_partitions_[0U], nullptr);

  const auto &partition = ctx.partition_datas_[0U];
  EXPECT_EQ(partition.type, ModelPartitionType::CUSTOM_OPS);
  EXPECT_EQ(partition.size, expected.size());
  ASSERT_NE(partition.data, nullptr);
  EXPECT_EQ(partition.data, ctx.owned_partitions_[0U]->data());

  const std::vector<uint8_t> actual(partition.data, partition.data + partition.size);
  EXPECT_EQ(actual, expected);
}

TEST_F(UtestOmFileHelper, AddOwnedPartitionInvalid) {
  OmFileSaveHelper saver;
  std::vector<uint8_t> payload;
  EXPECT_EQ(saver.AddOwnedPartition(ModelPartitionType::CUSTOM_OPS, std::move(payload), 0U), PARAM_INVALID);
}

TEST_F(UtestOmFileHelper, TestInvalidPartitionNumber) {
  std::vector<ModelPartitionType> valid0{MODEL_DEF};
  EXPECT_EQ(OmFileLoadHelper::CheckPartitionTableNum(valid0.size()), true);

  std::vector<ModelPartitionType> valid1{MODEL_DEF, WEIGHTS_DATA,       TASK_INFO,  TBE_KERNELS,
                                         SO_BINS,   CUST_AICPU_KERNELS, TILING_DATA};
  EXPECT_EQ(OmFileLoadHelper::CheckPartitionTableNum(valid1.size()), true);
}

// soc_version 为空时跳过兼容性检查，返回成功
TEST_F(UtestOmFileHelper, CheckModelCompatibility_EmptySocVersion_ReturnSuccess) {
  OmFileLoadHelper loader;
  Model model("test_model", "v1");
  EXPECT_EQ(loader.CheckModelCompatibility(model), SUCCESS);
}

// soc_version 非法时(用户传错), aclrtCheckArchCompatibility 返回失败, 应回报参数错误 PARAM_INVALID
TEST_F(UtestOmFileHelper, CheckModelCompatibility_InvalidSocVersion_ReturnParamInvalid) {
  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtCheckArchCompatibility(const char *socVersion, int32_t *canCompatible) override {
      if (canCompatible != nullptr) {
        *canCompatible = 0;
      }
      return static_cast<aclError>(-1);  // 模拟用户传入的 soc_version 非法
    }
  };
  ge::AclRuntimeStub::SetInstance(std::make_shared<MockAclRuntime>());

  OmFileLoadHelper loader;
  Model model("test_model", "v1");
  (void)AttrUtils::SetStr(model, "soc_version", "invalid_soc_version");
  EXPECT_EQ(loader.CheckModelCompatibility(model), PARAM_INVALID);

  ge::AclRuntimeStub::Reset();
}

// soc_version 合法但与当前设备不兼容时(canCompatible==0), 应返回失败
TEST_F(UtestOmFileHelper, CheckModelCompatibility_IncompatibleDevice_ReturnFailed) {
  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtCheckArchCompatibility(const char *socVersion, int32_t *canCompatible) override {
      if (canCompatible != nullptr) {
        *canCompatible = 0;  // 合法 soc_version, 但当前设备不兼容
      }
      return ACL_SUCCESS;
    }
  };
  ge::AclRuntimeStub::SetInstance(std::make_shared<MockAclRuntime>());

  OmFileLoadHelper loader;
  Model model("test_model", "v1");
  (void)AttrUtils::SetStr(model, "soc_version", "Ascend910A");
  EXPECT_NE(loader.CheckModelCompatibility(model), SUCCESS);

  ge::AclRuntimeStub::Reset();
}

TEST_F(UtestOmFileHelper, CheckPartitionTableNum_invalid) {
  EXPECT_EQ(OmFileLoadHelper::CheckPartitionTableNum(0U), false);
  EXPECT_EQ(OmFileLoadHelper::CheckPartitionTableNum(9U), false);
  EXPECT_EQ(OmFileLoadHelper::CheckPartitionTableNum(100U), false);
}

TEST_F(UtestOmFileHelper, GetModelPartitions_out_of_range_and_valid) {
  OmFileLoadHelper loader;
  loader.model_contexts_.emplace_back(OmFileContext{});
  auto &partitions = loader.GetModelPartitions(0U);
  EXPECT_EQ(partitions.empty(), true);
  const auto &empty = loader.GetModelPartitions(10U);
  EXPECT_EQ(empty.empty(), true);
}

TEST_F(UtestOmFileHelper, GetModelPartition_not_found_not_optional) {
  OmFileLoadHelper loader;
  loader.is_inited_ = true;
  loader.model_contexts_.emplace_back(OmFileContext{});
  ModelPartition partition;
  EXPECT_EQ(loader.GetModelPartition(MODEL_DEF, partition, 0U), FAILED);
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTable_data_too_small) {
  OmFileLoadHelper loader;
  uint8_t dummy[1] = {0};
  size_t mem_offset = 0U;
  EXPECT_EQ(loader.LoadModelPartitionTable(dummy, 1U, 0U, mem_offset, nullptr),
            ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTable_invalid_tiny_num) {
  OmFileLoadHelper loader;
  uint64_t total_size = sizeof(TinyModelPartitionTable) + sizeof(TinyModelPartitionMemInfo) + 100U;
  std::vector<uint8_t> data(total_size, 0);
  TinyModelPartitionTable *tiny_table = reinterpret_cast<TinyModelPartitionTable *>(data.data());
  tiny_table->num = 100U;
  size_t mem_offset = 0U;
  EXPECT_EQ(loader.LoadModelPartitionTable(data.data(), total_size, 0U, mem_offset, nullptr),
            ACL_ERROR_GE_PARAM_INVALID);
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTable_model_index_mismatch) {
  OmFileLoadHelper loader;
  loader.model_contexts_.emplace_back(OmFileContext{});
  uint32_t num = 1U;
  uint32_t partition_data_size = 10U;
  uint64_t table_size = sizeof(ModelPartitionTable) + sizeof(ModelPartitionMemInfo) * num;
  uint64_t total_size = table_size + partition_data_size;
  std::vector<uint8_t> data(total_size, 0);
  ModelPartitionTable *pt = reinterpret_cast<ModelPartitionTable *>(data.data());
  pt->num = num;
  pt->partition[0] = {MODEL_DEF, 0U, partition_data_size};
  ModelFileHeader header;
  header.modeltype = MODEL_TYPE_FLOW_MODEL;
  header.model_length = total_size;
  size_t mem_offset = 0U;
  EXPECT_EQ(loader.LoadModelPartitionTable(data.data(), total_size, 0U, mem_offset, &header), FAILED);
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTable_partition_size_too_large) {
  OmFileLoadHelper loader;
  uint32_t num = 1U;
  uint64_t table_size = sizeof(ModelPartitionTable) + sizeof(ModelPartitionMemInfo) * num;
  uint64_t total_size = table_size + 10U;
  std::vector<uint8_t> data(total_size, 0);
  ModelPartitionTable *pt = reinterpret_cast<ModelPartitionTable *>(data.data());
  pt->num = num;
  pt->partition[0] = {MODEL_DEF, 0U, 10000U};
  ModelFileHeader header;
  header.modeltype = MODEL_TYPE_FLOW_MODEL;
  header.model_length = total_size;
  size_t mem_offset = 0U;
  EXPECT_EQ(loader.LoadModelPartitionTable(data.data(), total_size, 0U, mem_offset, &header),
            ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);
}

TEST_F(UtestOmFileHelper, LoadModelPartitionTableWithNum_success_and_mismatch) {
  uint32_t num = 1U;
  uint32_t partition_data_size = 10U;
  uint64_t table_size = sizeof(ModelPartitionTable) + sizeof(ModelPartitionMemInfo) * num;
  uint64_t total_size = table_size + partition_data_size;
  std::vector<uint8_t> data(total_size, 0);
  ModelPartitionTable *pt = reinterpret_cast<ModelPartitionTable *>(data.data());
  pt->num = num;
  pt->partition[0] = {MODEL_DEF, 0U, partition_data_size};
  ModelFileHeader header;
  header.modeltype = MODEL_TYPE_FLOW_MODEL;
  header.model_length = total_size;

  OmFileLoadHelper loader1;
  EXPECT_EQ(loader1.LoadModelPartitionTable(data.data(), total_size, 1U, &header), SUCCESS);

  OmFileLoadHelper loader2;
  EXPECT_EQ(loader2.LoadModelPartitionTable(data.data(), total_size + 100U, 1U, &header), FAILED);
}

TEST_F(UtestOmFileHelper, Init_tiny_model_success) {
  uint32_t num = 1U;
  uint32_t partition_data_size = 10U;
  uint64_t tiny_table_size = sizeof(TinyModelPartitionTable) + sizeof(TinyModelPartitionMemInfo) * num;
  uint64_t total_size = tiny_table_size + partition_data_size;
  std::vector<uint8_t> data(total_size, 0);
  TinyModelPartitionTable *tiny_table = reinterpret_cast<TinyModelPartitionTable *>(data.data());
  tiny_table->num = num;
  tiny_table->partition[0] = {MODEL_DEF, 0U, partition_data_size};

  OmFileLoadHelper loader;
  EXPECT_EQ(loader.Init(data.data(), static_cast<uint32_t>(total_size)), SUCCESS);
  EXPECT_TRUE(loader.is_inited_);
}

TEST_F(UtestOmFileHelper, Init_with_model_num_success) {
  uint32_t num = 1U;
  uint32_t partition_data_size = 10U;
  uint64_t table_size = sizeof(ModelPartitionTable) + sizeof(ModelPartitionMemInfo) * num;
  uint64_t total_size = table_size + partition_data_size;
  std::vector<uint8_t> data(total_size, 0);
  ModelPartitionTable *pt = reinterpret_cast<ModelPartitionTable *>(data.data());
  pt->num = num;
  pt->partition[0] = {MODEL_DEF, 0U, partition_data_size};
  ModelFileHeader header;
  header.modeltype = MODEL_TYPE_FLOW_MODEL;
  header.model_length = total_size;

  OmFileLoadHelper loader;
  EXPECT_EQ(loader.Init(data.data(), total_size, 1U, &header), SUCCESS);
  EXPECT_TRUE(loader.is_inited_);
}

TEST_F(UtestOmFileHelper, AddPartition_index_overflow_and_size_overflow) {
  OmFileSaveHelper saver;
  ModelPartition partition;
  EXPECT_EQ(saver.AddPartition(partition, 2U), FAILED);

  EXPECT_EQ(saver.AddPartition(partition), SUCCESS);
  saver.model_contexts_[0U].model_data_len_ = static_cast<uint64_t>(-1);
  partition.size = 1U;
  EXPECT_EQ(saver.AddPartition(partition), FAILED);
}

TEST_F(UtestOmFileHelper, AddOwnedPartition_index_overflow) {
  OmFileSaveHelper saver;
  std::vector<uint8_t> payload = {0x12U, 0x34U};
  EXPECT_EQ(saver.AddOwnedPartition(ModelPartitionType::CUSTOM_OPS, std::move(payload), 2U), FAILED);
}

TEST_F(UtestOmFileHelper, SaveModel_empty_and_zero_len) {
  OmFileSaveHelper saver1;
  ModelBufferData model1;
  EXPECT_EQ(saver1.SaveModel(nullptr, model1, true), FAILED);

  OmFileSaveHelper saver2;
  OmFileContext oc;
  oc.model_data_len_ = 0U;
  saver2.model_contexts_.push_back(oc);
  ModelBufferData model2;
  EXPECT_EQ(saver2.SaveModel(nullptr, model2, true), PARAM_INVALID);
}

TEST_F(UtestOmFileHelper, SaveModel_save_to_buff_success) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  auto buff = reinterpret_cast<uint8_t *>(malloc(12));
  ModelPartition partition;
  partition.type = MODEL_DEF;
  partition.data = buff;
  partition.size = 12U;
  oc.partition_datas_.push_back(partition);
  oc.model_data_len_ = 12U;
  saver.model_contexts_.push_back(oc);
  ModelBufferData model;
  EXPECT_EQ(saver.SaveModel(nullptr, model, false), SUCCESS);
  free(buff);
}

TEST_F(UtestOmFileHelper, SaveModel_save_to_file_success) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  auto buff = reinterpret_cast<uint8_t *>(malloc(12));
  ModelPartition partition;
  partition.type = MODEL_DEF;
  partition.data = buff;
  partition.size = 12U;
  oc.partition_datas_.push_back(partition);
  oc.model_data_len_ = 12U;
  saver.model_contexts_.push_back(oc);
  ModelBufferData model;
  EXPECT_EQ(saver.SaveModel("./test_om_helper.om", model, true), SUCCESS);
  free(buff);
  system("rm -rf ./test_om_helper.om");
}

TEST_F(UtestOmFileHelper, CheckModelCompatibility_compatible) {
  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtCheckArchCompatibility(const char *socVersion, int32_t *canCompatible) override {
      if (canCompatible != nullptr) {
        *canCompatible = 1;
      }
      return ACL_SUCCESS;
    }
  };
  ge::AclRuntimeStub::SetInstance(std::make_shared<MockAclRuntime>());

  OmFileLoadHelper loader;
  Model model("test_model", "v1");
  (void)AttrUtils::SetStr(model, "soc_version", "Ascend910A");
  EXPECT_EQ(loader.CheckModelCompatibility(model), SUCCESS);

  ge::AclRuntimeStub::Reset();
}

TEST_F(UtestOmFileHelper, GetPartitionTable_no_args) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  oc.partition_datas_.push_back(ModelPartition());
  saver.model_contexts_.push_back(oc);
  EXPECT_NE(saver.GetPartitionTable(), nullptr);
}

TEST_F(UtestOmFileHelper, GetPartitionTable_overflow) {
  OmFileSaveHelper saver;
  OmFileContext oc;
  ModelPartition p1;
  p1.size = static_cast<uint64_t>(-1);
  oc.partition_datas_.push_back(p1);
  ModelPartition p2;
  p2.size = 1U;
  oc.partition_datas_.push_back(p2);
  saver.model_contexts_.push_back(oc);
  EXPECT_EQ(saver.GetPartitionTable(0U), nullptr);
}

}  // namespace ge
