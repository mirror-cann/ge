/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl/acl_rt.h"
#include "graph/tensor.h"
#include "common/checker.h"
#include "ge_api_c_wrapper_utils.h"
#include "graph/utils/tensor_adapter.h"
#include "graph/utils/tensor_utils.h"
#include "graph/utils/tensor_value_utils.h"
#include "graph/utils/type_utils.h"
#include <memory>

#ifdef __cplusplus

using namespace ge;
using namespace ge::c_wrapper;
extern "C" {
#endif
struct EsCTensor;

namespace {
size_t GetTensorStorageSize(const Tensor &tensor) {
  const auto data_size = tensor.GetSize();
  if (data_size != 0U) {
    return data_size;
  }

  const auto desc_size = tensor.GetTensorDesc().GetSize();
  if (desc_size > 0) {
    return static_cast<size_t>(desc_size);
  }

  const auto shape_size = tensor.GetTensorDesc().GetShape().GetShapeSize();
  if (shape_size <= 0) {
    return 0U;
  }

  uint32_t type_length = 0U;
  if (!TypeUtils::GetDataTypeLength(tensor.GetDataType(), type_length)) {
    return 0U;
  }
  return static_cast<size_t>(shape_size) * type_length;
}
}  // namespace

graphStatus GeApiWrapper_Tensor_SetFormat(EsCTensor *tensor, const ge::Format &format) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  return ts->SetFormat(format);
}

ge::Format GeApiWrapper_Tensor_GetFormat(EsCTensor *tensor) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  return ts->GetFormat();
}

graphStatus GeApiWrapper_Tensor_SetDataType(EsCTensor *tensor, const ge::DataType &dtype) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  return ts->SetDataType(dtype);
}

ge::DataType GeApiWrapper_Tensor_GetDataType(EsCTensor *tensor) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  return ts->GetDataType();
}

int32_t GeApiWrapper_Tensor_GetPlacement(EsCTensor *tensor) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  return static_cast<int32_t>(ts->GetPlacement());
}

graphStatus GeApiWrapper_Tensor_ToHost(EsCTensor *tensor) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  const auto desc = ts->GetTensorDesc();
  const auto ge_desc = TensorAdapter::TensorDesc2GeTensorDesc(desc);
  int64_t mem_size = -1;
  // 内存大小可能加了padding，根据shape重新计算大小，申请host内存
  GE_ASSERT_GRAPH_SUCCESS(
      TensorUtils::CalcTensorMemSize(ge_desc.GetShape(), ge_desc.GetFormat(), ge_desc.GetDataType(), mem_size));
  GE_ASSERT_TRUE(mem_size >= 0L);
  const auto size = static_cast<size_t>(mem_size);
  if (ts->GetSize() < size) {
    return GRAPH_FAILED;
  }

  if (size == 0U) {
    const uint8_t *empty_data = nullptr;
    const auto ret = ts->SetData(empty_data, 0U);
    if (ret != GRAPH_SUCCESS) {
      return ret;
    }
    return ts->SetPlacement(ge::Placement::kPlacementHost);
  }

  const auto *device_ptr = ts->GetData();
  if (device_ptr == nullptr) {
    return GRAPH_FAILED;
  }

  auto *host_data = new (std::nothrow) uint8_t[size];
  if (host_data == nullptr) {
    return GRAPH_FAILED;
  }

  const auto memcpy_ret = aclrtMemcpy(host_data, size, device_ptr, size, ACL_MEMCPY_DEVICE_TO_HOST);
  if (memcpy_ret != ACL_SUCCESS) {
    delete[] host_data;
    return GRAPH_FAILED;
  }

  const auto ret = ts->SetData(host_data, size, [](uint8_t *ptr) {
    delete[] ptr;
  });
  if (ret != GRAPH_SUCCESS) {
    delete[] host_data;
    return ret;
  }
  return ts->SetPlacement(ge::Placement::kPlacementHost);
}

graphStatus GeApiWrapper_Tensor_ToDevice(EsCTensor *tensor) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  const auto size = GetTensorStorageSize(*ts);
  if (size == 0U) {
    return ts->SetPlacement(ge::Placement::kPlacementDevice);
  }

  void *device_ptr = nullptr;
  const auto malloc_ret = aclrtMalloc(&device_ptr, size, ACL_MEM_MALLOC_NORMAL_ONLY);
  if (malloc_ret != ACL_SUCCESS) {
    return GRAPH_FAILED;
  }

  const auto *host_ptr = ts->GetData();
  if (host_ptr != nullptr) {
    const auto memcpy_ret = aclrtMemcpy(device_ptr, size, host_ptr, size, ACL_MEMCPY_HOST_TO_DEVICE);
    if (memcpy_ret != ACL_SUCCESS) {
      (void)aclrtFree(device_ptr);
      return GRAPH_FAILED;
    }
  }

  const auto ret = ts->SetData(static_cast<uint8_t *>(device_ptr), size, [](uint8_t *ptr) {
    if (ptr == nullptr) {
      return;
    }
    (void)aclrtFree(static_cast<void *>(ptr));
  });
  if (ret != GRAPH_SUCCESS) {
    (void)aclrtFree(device_ptr);
    return ret;
  }
  return ts->SetPlacement(ge::Placement::kPlacementDevice);
}

EsCTensor *GeApiWrapper_Tensor_CreateTensor() {
  auto tensor = new (std::nothrow) ge::Tensor();
  GE_ASSERT_NOTNULL(tensor);
  return static_cast<EsCTensor *>(static_cast<void *>(tensor));
}

void GeApiWrapper_Tensor_DestroyEsCTensor(EsCTensor *tensor) {
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  delete ts;
}

graphStatus GeApiWrapper_Tensor_GetShape(EsCTensor *tensor, int64_t **dims, size_t *dims_num) {
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  auto dims_vec = ts->GetTensorDesc().GetShape().GetDims();
  *dims = VecDimsToArray(dims_vec, dims_num);
  return GRAPH_SUCCESS;
}

void GeApiWrapper_Tensor_FreeDimsArray(int64_t *dims) {
  delete[] dims;
}

const char *GeApiWrapper_Tensor_GetData(EsCTensor *tensor) {
  GE_ASSERT_NOTNULL(tensor);
  auto *ts = static_cast<Tensor *>(static_cast<void *>(tensor));
  auto data_type = ts->GetDataType();
  auto str_val = TensorValueUtils::ConvertTensorValue(*ts, data_type, ",", false);
  AscendString asc(str_val.c_str());
  return AscendStringToChar(asc);
}
#ifdef __cplusplus
}
#endif
