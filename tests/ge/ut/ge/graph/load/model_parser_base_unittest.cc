/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <gtest/gtest.h>
#include "macro_utils/dt_public_scope.h"
#include "common/helper/model_parser_base.h"
#include "framework/omg/ge_init.h"
#include "common/model/ge_model.h"
#include "graph/buffer/buffer_impl.h"
#include "graph/utils/graph_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "macro_utils/dt_public_unscope.h"

#include "proto/task.pb.h"

using namespace std;

namespace ge {
class UtestModelParserBase : public testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UtestModelParserBase, LoadFromFile) {
  ModelParserBase base;
  ge::ModelData model_data;
  EXPECT_EQ(base.LoadFromFile("/tmp/123test", 1, model_data), ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID);
  EXPECT_EQ(base.LoadFromFile("", 1, model_data), ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID);
}

TEST_F(UtestModelParserBase, LoadFromFile_s) {
  system("touch test_xxx");
  ModelParserBase base;
  ge::ModelData model_data;

  EXPECT_EQ(base.LoadFromFile("./test_xxx", 1, model_data), ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID);
  system("rm -f ./test_xxx");
}

TEST_F(UtestModelParserBase, ParseModelContent) {
  ModelParserBase base;
  ge::ModelData model;
  uint8_t *modelData = nullptr;
  uint64_t model_len;
  EXPECT_EQ(base.ParseModelContent(model, modelData, model_len), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetModelInputDesc) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetModelInputDesc(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetModelOutputDesc) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetModelOutputDesc(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDynamicBatch) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicBatch(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDynamicHW) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicHW(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDynamicDims) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicDims(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDataNameOrder) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ModelInOutInfo info;
  EXPECT_EQ(base.GetDataNameOrder(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDynamicOutShape) {
  ModelParserBase base;
  uint8_t *data = nullptr;
  size_t size = 0U;
  ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicOutShape(data, size, info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, IsDynamicModel) {
  ModelFileHeader header;
  header.version = 0U;
  header.model_num = 1U;
  header.is_unknow_model = 0U;
  EXPECT_FALSE(ModelParserBase::IsDynamicModel(header));

  header.version = MODEL_VERSION;
  header.model_num = 1U;
  header.is_unknow_model = 0U;
  EXPECT_FALSE(ModelParserBase::IsDynamicModel(header));

  header.version = MODEL_VERSION;
  header.model_num = 2U;
  header.is_unknow_model = 0U;
  EXPECT_TRUE(ModelParserBase::IsDynamicModel(header));

  header.version = MODEL_VERSION;
  header.model_num = 1U;
  header.is_unknow_model = 1U;
  EXPECT_TRUE(ModelParserBase::IsDynamicModel(header));
}

TEST_F(UtestModelParserBase, ParseModelContent_Success) {
  ModelParserBase base;
  const size_t header_size = sizeof(ModelFileHeader);
  const size_t payload_size = 100U;
  const size_t total_size = header_size + payload_size;
  vector<uint8_t> buffer(total_size, 0);

  auto *file_header = reinterpret_cast<ModelFileHeader *>(buffer.data());
  file_header->magic = MODEL_FILE_MAGIC_NUM;
  file_header->model_length = 0UL;
  file_header->length = static_cast<uint32_t>(payload_size);

  ge::ModelData model;
  model.model_data = buffer.data();
  model.model_len = total_size;
  model.om_name = "test_om";

  uint8_t *model_data = nullptr;
  uint64_t model_len = 0U;
  EXPECT_EQ(base.ParseModelContent(model, model_data, model_len), SUCCESS);
  EXPECT_EQ(model_len, payload_size);
  EXPECT_NE(model_data, nullptr);
}

TEST_F(UtestModelParserBase, ParseModelContent_ModelLengthNonZero) {
  ModelParserBase base;
  const size_t header_size = sizeof(ModelFileHeader);
  const size_t payload_size = 100U;
  const size_t total_size = header_size + payload_size;
  vector<uint8_t> buffer(total_size, 0);

  auto *file_header = reinterpret_cast<ModelFileHeader *>(buffer.data());
  file_header->magic = MODEL_FILE_MAGIC_NUM;
  file_header->model_length = static_cast<uint64_t>(payload_size);
  file_header->length = 0U;

  ge::ModelData model;
  model.model_data = buffer.data();
  model.model_len = total_size;
  model.om_name = "test_om";

  uint8_t *model_data = nullptr;
  uint64_t model_len = 0U;
  EXPECT_EQ(base.ParseModelContent(model, model_data, model_len), SUCCESS);
  EXPECT_EQ(model_len, payload_size);
}

TEST_F(UtestModelParserBase, ParseModelContent_TooSmall) {
  ModelParserBase base;
  vector<uint8_t> buffer(10U, 0);
  ge::ModelData model;
  model.model_data = buffer.data();
  model.model_len = buffer.size();
  model.om_name = "test_om";

  uint8_t *model_data = nullptr;
  uint64_t model_len = 0U;
  EXPECT_EQ(base.ParseModelContent(model, model_data, model_len), ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);
}

TEST_F(UtestModelParserBase, ParseModelContent_InvalidMagic) {
  ModelParserBase base;
  const size_t header_size = sizeof(ModelFileHeader);
  const size_t payload_size = 100U;
  const size_t total_size = header_size + payload_size;
  vector<uint8_t> buffer(total_size, 0);

  auto *file_header = reinterpret_cast<ModelFileHeader *>(buffer.data());
  file_header->magic = 0x12345678U;
  file_header->model_length = 0UL;
  file_header->length = static_cast<uint32_t>(payload_size);

  ge::ModelData model;
  model.model_data = buffer.data();
  model.model_len = total_size;
  model.om_name = "test_om";

  uint8_t *model_data = nullptr;
  uint64_t model_len = 0U;
  EXPECT_EQ(base.ParseModelContent(model, model_data, model_len), ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);
}

TEST_F(UtestModelParserBase, ParseModelContent_LengthMismatch) {
  ModelParserBase base;
  const size_t header_size = sizeof(ModelFileHeader);
  const size_t payload_size = 100U;
  const size_t total_size = header_size + payload_size;
  vector<uint8_t> buffer(total_size, 0);

  auto *file_header = reinterpret_cast<ModelFileHeader *>(buffer.data());
  file_header->magic = MODEL_FILE_MAGIC_NUM;
  file_header->model_length = 0UL;
  file_header->length = 200U;

  ge::ModelData model;
  model.model_data = buffer.data();
  model.model_len = total_size;
  model.om_name = "test_om";

  uint8_t *model_data = nullptr;
  uint64_t model_len = 0U;
  EXPECT_EQ(base.ParseModelContent(model, model_data, model_len), ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);
}

TEST_F(UtestModelParserBase, GetModelInputDesc_Success) {
  ModelParserBase base;
  const string name = "input_tensor";
  const int64_t dims[] = {1, 3, 224, 224};
  const int64_t dimsV2[] = {1, 3, 224, 224};
  const std::pair<int64_t, int64_t> shape_ranges[] = {{1, 1}, {3, 3}, {224, 224}, {224, 224}};

  ModelTensorDescBaseInfo base_info;
  base_info.size = 100U;
  base_info.format = FORMAT_NCHW;
  base_info.dt = DT_FLOAT;
  base_info.name_len = static_cast<uint32_t>(name.size());
  base_info.dims_len = static_cast<uint32_t>(sizeof(dims));
  base_info.dimsV2_len = static_cast<uint32_t>(sizeof(dimsV2));
  base_info.shape_range_len = static_cast<uint32_t>(sizeof(shape_ranges));

  const uint32_t desc_num = 1U;
  const size_t total_size = sizeof(uint32_t) + sizeof(ModelTensorDescBaseInfo) + name.size() + sizeof(dims) +
                            sizeof(dimsV2) + sizeof(shape_ranges);
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &desc_num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, &base_info, sizeof(ModelTensorDescBaseInfo));
  offset += sizeof(ModelTensorDescBaseInfo);
  memcpy(data.data() + offset, name.c_str(), name.size());
  offset += name.size();
  memcpy(data.data() + offset, dims, sizeof(dims));
  offset += sizeof(dims);
  memcpy(data.data() + offset, dimsV2, sizeof(dimsV2));
  offset += sizeof(dimsV2);
  memcpy(data.data() + offset, shape_ranges, sizeof(shape_ranges));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetModelInputDesc(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.input_desc.size(), 1U);
  EXPECT_EQ(info.input_desc[0].name, name);
  ASSERT_EQ(info.input_desc[0].dims.size(), 4U);
  EXPECT_EQ(info.input_desc[0].dims[0], 1);
  ASSERT_EQ(info.input_desc[0].dimsV2.size(), 4U);
  EXPECT_EQ(info.input_desc[0].dimsV2[3], 224);
  ASSERT_EQ(info.input_desc[0].shape_ranges.size(), 4U);
  EXPECT_EQ(info.input_desc[0].shape_ranges[0].first, 1);
  EXPECT_EQ(info.input_desc[0].shape_ranges[0].second, 1);
}

TEST_F(UtestModelParserBase, GetModelOutputDesc_Success) {
  ModelParserBase base;
  const string name = "output_tensor";

  ModelTensorDescBaseInfo base_info;
  base_info.size = 200U;
  base_info.format = FORMAT_ND;
  base_info.dt = DT_INT32;
  base_info.name_len = static_cast<uint32_t>(name.size());
  base_info.dims_len = 0U;
  base_info.dimsV2_len = 0U;
  base_info.shape_range_len = 0U;

  const uint32_t desc_num = 1U;
  const size_t total_size = sizeof(uint32_t) + sizeof(ModelTensorDescBaseInfo) + name.size();
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &desc_num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, &base_info, sizeof(ModelTensorDescBaseInfo));
  offset += sizeof(ModelTensorDescBaseInfo);
  memcpy(data.data() + offset, name.c_str(), name.size());

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetModelOutputDesc(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.output_desc.size(), 1U);
  EXPECT_EQ(info.output_desc[0].name, name);
  EXPECT_TRUE(info.output_desc[0].dims.empty());
  EXPECT_TRUE(info.output_desc[0].dimsV2.empty());
  EXPECT_TRUE(info.output_desc[0].shape_ranges.empty());
}

TEST_F(UtestModelParserBase, GetModelInputDesc_ZeroDescNum) {
  ModelParserBase base;
  const uint32_t desc_num = 0U;
  vector<uint8_t> data(sizeof(uint32_t), 0);
  memcpy(data.data(), &desc_num, sizeof(uint32_t));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetModelInputDesc(data.data(), data.size(), info), SUCCESS);
  EXPECT_TRUE(info.input_desc.empty());
}

TEST_F(UtestModelParserBase, GetModelInputDesc_SizeTooSmall) {
  ModelParserBase base;
  const uint32_t desc_num = 1U;
  vector<uint8_t> data(sizeof(uint32_t) + 4U, 0);
  memcpy(data.data(), &desc_num, sizeof(uint32_t));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetModelInputDesc(data.data(), data.size(), info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDynamicBatch_Success) {
  ModelParserBase base;
  const uint32_t num = 3U;
  const uint64_t batches[] = {1U, 2U, 4U};
  const size_t total_size = sizeof(uint32_t) + sizeof(batches);
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, batches, sizeof(batches));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicBatch(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.dynamic_batch.size(), 3U);
  EXPECT_EQ(info.dynamic_batch[0], 1U);
  EXPECT_EQ(info.dynamic_batch[1], 2U);
  EXPECT_EQ(info.dynamic_batch[2], 4U);
}

TEST_F(UtestModelParserBase, GetDynamicBatch_SizeTooSmall) {
  ModelParserBase base;
  const uint32_t num = 2U;
  vector<uint8_t> data(sizeof(uint32_t) + sizeof(uint64_t), 0);
  memcpy(data.data(), &num, sizeof(uint32_t));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicBatch(data.data(), data.size(), info), PARAM_INVALID);
}

TEST_F(UtestModelParserBase, GetDynamicHW_Success) {
  ModelParserBase base;
  const uint32_t num = 2U;
  const uint32_t second_vec[] = {2U, 1U};
  const uint64_t hw1[] = {224U, 224U};
  const uint64_t hw2[] = {112U};
  const size_t total_size = sizeof(uint32_t) + sizeof(second_vec) + sizeof(hw1) + sizeof(hw2);
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, second_vec, sizeof(second_vec));
  offset += sizeof(second_vec);
  memcpy(data.data() + offset, hw1, sizeof(hw1));
  offset += sizeof(hw1);
  memcpy(data.data() + offset, hw2, sizeof(hw2));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicHW(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.dynamic_hw.size(), 2U);
  ASSERT_EQ(info.dynamic_hw[0].size(), 2U);
  EXPECT_EQ(info.dynamic_hw[0][0], 224U);
  EXPECT_EQ(info.dynamic_hw[0][1], 224U);
  ASSERT_EQ(info.dynamic_hw[1].size(), 1U);
  EXPECT_EQ(info.dynamic_hw[1][0], 112U);
}

TEST_F(UtestModelParserBase, GetDynamicDims_Success) {
  ModelParserBase base;
  const uint32_t num = 1U;
  const uint32_t second_vec[] = {3U};
  const uint64_t dims[] = {1U, 2U, 3U};
  const size_t total_size = sizeof(uint32_t) + sizeof(second_vec) + sizeof(dims);
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, second_vec, sizeof(second_vec));
  offset += sizeof(second_vec);
  memcpy(data.data() + offset, dims, sizeof(dims));

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicDims(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.dynamic_dims.size(), 1U);
  ASSERT_EQ(info.dynamic_dims[0].size(), 3U);
  EXPECT_EQ(info.dynamic_dims[0][0], 1U);
  EXPECT_EQ(info.dynamic_dims[0][2], 3U);
}

TEST_F(UtestModelParserBase, GetDataNameOrder_Success) {
  ModelParserBase base;
  const uint32_t num = 2U;
  const uint32_t strlens[] = {5U, 3U};
  const string str1 = "input";
  const string str2 = "out";
  const size_t total_size = sizeof(uint32_t) + sizeof(strlens) + str1.size() + str2.size();
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, strlens, sizeof(strlens));
  offset += sizeof(strlens);
  memcpy(data.data() + offset, str1.c_str(), str1.size());
  offset += str1.size();
  memcpy(data.data() + offset, str2.c_str(), str2.size());

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDataNameOrder(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.data_name_order.size(), 2U);
  EXPECT_EQ(info.data_name_order[0], "input");
  EXPECT_EQ(info.data_name_order[1], "out");
}

TEST_F(UtestModelParserBase, GetDynamicOutShape_Success) {
  ModelParserBase base;
  const uint32_t num = 1U;
  const uint32_t strlens[] = {6U};
  const string str1 = "-1,224";
  const size_t total_size = sizeof(uint32_t) + sizeof(strlens) + str1.size();
  vector<uint8_t> data(total_size, 0);
  size_t offset = 0U;
  memcpy(data.data() + offset, &num, sizeof(uint32_t));
  offset += sizeof(uint32_t);
  memcpy(data.data() + offset, strlens, sizeof(strlens));
  offset += sizeof(strlens);
  memcpy(data.data() + offset, str1.c_str(), str1.size());

  ge::ModelInOutInfo info;
  EXPECT_EQ(base.GetDynamicOutShape(data.data(), data.size(), info), SUCCESS);
  ASSERT_EQ(info.dynamic_output_shape.size(), 1U);
  EXPECT_EQ(info.dynamic_output_shape[0], "-1,224");
}
}  // namespace ge
