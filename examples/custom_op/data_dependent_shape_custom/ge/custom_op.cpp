/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include "acl/acl_rt.h"
#include "graph/custom_op.h"
#include "where_like_custom.h"

extern "C" void LaunchWhereLikeCustom(const void *x, void *y, void *shape_buffer, uint64_t element_count,
                                            void *stream);

#define CHECK_ACL(x)                                                                  \
  do {                                                                                \
    aclError __ret = x;                                                               \
    if (__ret != ACL_ERROR_NONE) {                                                    \
      std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
      return ge::GRAPH_FAILED;                                                        \
    }                                                                                 \
  } while (0)

namespace {
constexpr size_t kInputIndex = 0U;
constexpr size_t kOutputIndex = 0U;
constexpr int64_t kUnknownDim = -1;
constexpr int64_t kIndexRank = 1;
constexpr size_t kOutputRank = 2U;
constexpr size_t kShapeBufferElementCount = 1U + kOutputRank;

bool IsUnknownRank(const gert::Shape *shape) {
  return (shape == nullptr) || (shape->GetDimNum() == 0U);
}

int64_t GetIndexRank(const gert::Shape *shape) {
  if (IsUnknownRank(shape)) {
    return kUnknownDim;
  }
  return static_cast<int64_t>(shape->GetDimNum());
}

ge::graphStatus UpdateOutputShapeFromDeviceBuffer(gert::Tensor *output, const void *shape_buffer) {
  if ((output == nullptr) || (shape_buffer == nullptr)) {
    return ge::GRAPH_FAILED;
  }

  uint64_t host_shape_buffer[kShapeBufferElementCount] = {};
  CHECK_ACL(aclrtMemcpy(host_shape_buffer, sizeof(host_shape_buffer), shape_buffer, sizeof(host_shape_buffer),
                        ACL_MEMCPY_DEVICE_TO_HOST));

  const uint64_t dim_num = host_shape_buffer[0];
  if (dim_num != kOutputRank) {
    std::cerr << __FILE__ << ":" << __LINE__ << " invalid output dim num: " << dim_num << std::endl;
    return ge::GRAPH_FAILED;
  }

  gert::Shape actual_shape;
  for (uint64_t i = 0U; i < dim_num; ++i) {
    actual_shape.AppendDim(static_cast<int64_t>(host_shape_buffer[i + 1U]));
  }

  const int64_t actual_size = ge::GetSizeInBytes(actual_shape.GetShapeSize(), output->GetDataType());
  if ((actual_size < 0) || (static_cast<size_t>(actual_size) > output->GetSize())) {
    std::cerr << __FILE__ << ":" << __LINE__ << " invalid actual output size: " << actual_size
              << ", max output size: " << output->GetSize() << std::endl;
    return ge::GRAPH_FAILED;
  }

  output->MutableOriginShape() = actual_shape;
  output->MutableStorageShape() = actual_shape;
  output->MutableTensorData().SetSize(static_cast<size_t>(actual_size));
  return ge::GRAPH_SUCCESS;
}
}  // namespace

namespace ge {
class WhereLikeCustom : public EagerExecuteOp, public ShapeInferOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    if (ctx == nullptr) {
      std::cerr << __FILE__ << ":" << __LINE__ << " execute context is null" << std::endl;
      return GRAPH_FAILED;
    }

    const gert::Tensor *input_x = ctx->GetInputTensor(kInputIndex);
    if (input_x == nullptr) {
      std::cerr << __FILE__ << ":" << __LINE__ << " input tensor is null" << std::endl;
      return GRAPH_FAILED;
    }
    const int64_t element_count = input_x->GetShapeSize();
    if (element_count < 0) {
      std::cerr << __FILE__ << ":" << __LINE__ << " invalid input shape size: " << element_count << std::endl;
      return GRAPH_FAILED;
    }
    long origin_dim_num = input_x->GetOriginShape().GetDimNum();
    long storage_dim_num = input_x->GetStorageShape().GetDimNum();
    const gert::StorageShape max_shape({element_count, origin_dim_num}, {element_count, storage_dim_num});
    const gert::StorageFormat &format = input_x->GetFormat();
    gert::Tensor *output_y = ctx->MallocOutputTensor(kOutputIndex, max_shape, format, DT_INT64);
    void *shape_buffer = ctx->MallocWorkSpace(kShapeBufferElementCount * sizeof(uint64_t));
    if ((output_y == nullptr) || (shape_buffer == nullptr)) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to prepare output, output_y: " << output_y
                << ", shape_buffer: " << shape_buffer << std::endl;
      return GRAPH_FAILED;
    }

    LaunchWhereLikeCustom(input_x->GetAddr(), output_y->GetAddr(), shape_buffer,
                          static_cast<uint64_t>(element_count), ctx->GetStream());
    CHECK_ACL(aclrtSynchronizeStream(ctx->GetStream()));
    return UpdateOutputShapeFromDeviceBuffer(output_y, shape_buffer);
  }

  // 编译期 shape 推导：Where 输出为二维索引张量，第 0 维依赖输入数据内容，需由 Execute 阶段通过
  // shape buffer 回写实际值；这里只能给出 unknown 维和可静态确定的输入 rank。
  graphStatus InferShape(gert::InferShapeContext *ctx) override {
    const auto *const input_shape = ctx->GetInputShape(kInputIndex);
    auto *const output_shape = ctx->GetOutputShape(kOutputIndex);
    if (output_shape == nullptr) {
      return GRAPH_FAILED;
    }

    // 输出为二维索引张量，形状为 [命中元素个数, 输入rank]。
    // 命中元素个数依赖运行时输入数据，编译期无法确定，因此第0维保持 unknown。
    output_shape->SetDimNum(0U);
    output_shape->AppendDim(kUnknownDim);
    output_shape->AppendDim(kIndexRank);
    output_shape->SetDim(1U, GetIndexRank(input_shape));
    return GRAPH_SUCCESS;
  }

  graphStatus InferDataType(gert::InferDataTypeContext *ctx) override {
    return ctx->SetOutputDataType(kOutputIndex, DT_INT64);
  }
};

REG_AUTO_MAPPING_OP(WhereLikeCustom);
}  // namespace ge
