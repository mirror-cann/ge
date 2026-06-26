/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "tensor_desc_internal.h"
#include <sstream>
#include "framework/common/ge_format_util.h"
#include "utils/acl_string_utils.h"
#include "utils/math_utils.h"
#include "model/acl_resource_manager.h"
#include "common/prof_api_reg.h"
#include "common/error_codes_inner.h"
#include "model/acl_model_impl.h"
#include "model/acl_model_impl_om2.h"
#include "model/acl_resource_manager_om2.h"

namespace acl {
void ConvertSvecToVec(const ge::SmallVector<int64_t, static_cast<size_t>(ge::kDefaultMaxInputNum)> &svec,
                      std::vector<int64_t> &vec) {
  vec.resize(svec.size());
  for (size_t i = 0U; i < vec.size(); ++i) {
    vec[i] = svec[i];
  }
}

void ConvertVecToSvec(const std::vector<int64_t> &vec,
                      ge::SmallVector<int64_t, static_cast<size_t>(ge::kDefaultMaxInputNum)> &svec) {
  svec.resize(vec.size());
  for (size_t i = 0U; i < vec.size(); ++i) {
    svec[i] = vec[i];
  }
}
}  // namespace acl

aclError aclTransTensorDescFormatImpl(const aclTensorDesc *srcDesc, aclFormat dstFormat, aclTensorDesc **dstDesc) {
  ACL_PROFILING_REG(acl::AclProfType::AclTransTensorDescFormat);
  ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(srcDesc);
  ACL_REQUIRES_NOT_NULL_WITH_INPUT_REPORT(dstDesc);

  std::vector<int64_t> dims;
  acl::ConvertSvecToVec(srcDesc->dims, dims);
  const ge::Shape shape(dims);
  const auto srcFormat = static_cast<ge::Format>(srcDesc->format);
  const auto dataType = static_cast<ge::DataType>(srcDesc->dataType);
  const ge::TensorDesc desc(shape, srcFormat, dataType);

  std::vector<int64_t> dstShape;
  const auto geRet = ge::GeFormatUtil::TransShape(desc, static_cast<ge::Format>(dstFormat), dstShape);
  if (geRet != ge::SUCCESS) {
    ACL_LOG_CALL_ERROR("[Call][TransShape]invoke TransShape failed. ge result = %u", geRet);
    return ACL_GET_ERRCODE_GE(geRet);
  }

  *dstDesc = aclCreateTensorDescImplOm2(srcDesc->dataType, static_cast<int32_t>(dstShape.size()), dstShape.data(),
                                        srcDesc->format);
  if (*dstDesc == nullptr) {
    ACL_LOG_INNER_ERROR("[Create][Desc]aclCreateTensorDesc failed.");
    return ACL_ERROR_BAD_ALLOC;
  }

  return ACL_SUCCESS;
}
