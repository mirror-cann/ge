/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACL_MODEL_SRC_MODEL_MODEL_COMMON_H_
#define ACL_MODEL_SRC_MODEL_MODEL_COMMON_H_

#include <vector>
#include <memory>
#include <string>
#include <set>
#include "acl/acl_base.h"
#include "acl/acl_mdl.h"
#include "framework/runtime/rt_session.h"

// Common enums for tensor/dims operations
enum class TensorType : std::uint8_t { INPUT_TENSOR_TYPE = 0, OUTPUT_TENSOR_TYPE };

enum class DimsType : std::uint8_t { DIMS_TYPE_V1 = 0, DIMS_TYPE_V2 };

namespace acl {
aclError GetDynamicTensorInfoHelp(aclmdlDesc *const modelDesc, const int32_t dynamicType,
                                  const std::vector<std::vector<int64_t>> &batchInfo);

aclError GetCurGearIndex(const aclmdlDesc *const modelDesc, const std::vector<uint64_t> &shapeInfo,
                         const int32_t dynamicType, size_t &curGearIndex);

aclError GetCurOuputShapeInfo(const aclmdlDesc *const modelDesc, const size_t index, const size_t curGearIndex,
                              aclmdlIODims *const dims);

aclError GetModelOutputShapeInfoHelp(aclmdlDesc *const modelDesc, std::vector<std::string> &geDynamicOutputShape);

bool TransConvertTensorNameToLegal(const aclmdlDesc *const modelDesc, std::string &tensorName);

void GetConvertTensorName(const aclmdlDesc *const modelDesc, const size_t idx, const TensorType tensorType,
                          std::string &convertName);

aclError GetTensorDescNameToDims(const aclmdlDesc *const modelDesc, const std::string &realName,
                                 const TensorType tensorType, const size_t idx, aclmdlIODims *const dims);

aclError GetDims(const aclmdlDesc *const modelDesc, const TensorType tensorType, const DimsType dimsType,
                 const size_t idx, aclmdlIODims *const dims);

const char_t *GetRealTensorName(const aclmdlDesc *const modelDesc, const std::string &tensorName);

// Bundle 子模型信息
struct BundleSubModelInfo {
  size_t workSize = 0U;
  size_t weightSize = 0U;
  size_t offset = 0U;
  size_t modelSize = 0U;
};

// Bundle 模型信息
struct BundleModelInfo {
  bool isInit = false;  // aclmdlBundleInit scene means true
  std::shared_ptr<gert::RtSession> rtSession;
  size_t varSize = 0U;
  std::string fromFilePath;
  std::shared_ptr<const uint8_t> bundleModelData;
  size_t bundleModelSize = 0U;
  std::vector<BundleSubModelInfo> subModelInfos;
  std::vector<uint32_t> loadedSubModelId;  // aclmdlBundleGetModelId use this when aclmdlBundleLoadFromxx is called
  std::set<uint32_t> loadedSubModelIdSet;
};

// Tensor name conversion constants
constexpr const char_t *TENSOR_NAME_PREFIX = "acl";
constexpr const char_t *TENSOR_INPUT_STR = "input";
constexpr const char_t *TENSOR_OUTPUT_STR = "output";
constexpr const char_t *MODEL_ID_STR = "modelId";
constexpr size_t TENSOR_NAME_ATTR_NUM = 5U;

// FP16 constants shared by acl_aipp.cpp and acl_aipp_om2.cpp
constexpr int16_t FP16_MAX_EXP = 0x001F;
constexpr int16_t FP16_MAX_MAN = 0x03FF;
constexpr int16_t FP16_MAN_HIDE_BIT = 0x0400;
constexpr int16_t FP16_SIGN_INDEX = 15;
constexpr uint32_t FP32_SIGN_MASK = 0x80000000U;
constexpr uint32_t FP32_SIGN_INDEX = 31U;
constexpr uint32_t FP32_EXP_MASK = 0x7F800000U;
constexpr uint32_t FP32_MAN_LEN = 23U;
constexpr uint32_t FP16_MAN_LEN = 10U;
constexpr uint32_t FP32_MAN_MASK = 0x007FFFFFU;
constexpr uint32_t FP32_MAN_HIDE_BIT = 0x00800000U;
constexpr uint32_t FP16_MAN_MASK = 0x03FFU;
constexpr uint32_t FP32_EXP_BIAS = 127U;
constexpr uint32_t FP16_EXP_BIAS = 15U;
constexpr uint32_t FP32_MAX_MAN = 0x7FFFFFU;

// AIPP parameter range constants
constexpr int8_t AIPP_SWITCH_ON = 1;
constexpr int8_t AIPP_SWITCH_OFF = 0;
constexpr int16_t CSC_MATRIX_MIN = -32677;
constexpr int16_t CSC_MATRIX_MAX = 32676;
constexpr int32_t IMAGE_SIZE_MIN = 1;
constexpr int32_t IMAGE_SIZE_MAX = 4096;
constexpr int32_t SCF_SIZE_MIN = 16;
constexpr int32_t SCF_SIZEW_MAX = 1920;
constexpr int32_t PADDING_MIN = 0;
constexpr int32_t PADDING_MAX = 32;
constexpr int16_t MEAN_CHN_MIN = 0;
constexpr int16_t MEAN_CHN_MAX = 255;
constexpr float32_t MIN_CHN_MIN = 0.0F;
constexpr float32_t MIN_CHN_MAX = 255.0F;
constexpr float32_t VR_CHN_MIN = -65504.0F;
constexpr float32_t VR_CHN_MAX = 65504.0F;

inline bool IsRoundOne(const uint64_t man, const uint16_t truncLen) {
  const uint16_t shiftOut = truncLen - 2U;
  uint64_t mask = 0x4U;
  uint64_t mask1 = 0x2U;
  uint64_t mask2;

  mask = mask << static_cast<uint64_t>(shiftOut);
  mask1 = mask1 << static_cast<uint64_t>(shiftOut);
  mask2 = mask1 - 1U;
  const bool lastBit = ((man & mask) > 0U);
  const bool truncHigh = ((man & mask1) > 0U);
  const bool truncLeft = ((man & mask2) > 0U);
  return (truncHigh && (truncLeft || lastBit));
}

inline void Fp16Normalize(int16_t &expo, uint16_t &man) {
  if (expo >= FP16_MAX_EXP) {
    expo = FP16_MAX_EXP - 1;
    man = static_cast<uint16_t>(FP16_MAX_MAN);
  }
  if ((expo == 0) && (static_cast<int16_t>(man) == FP16_MAN_HIDE_BIT)) {
    expo++;
    man = 0U;
  }
}
}  // namespace acl

#endif  // ACL_MODEL_SRC_MODEL_MODEL_COMMON_H_
