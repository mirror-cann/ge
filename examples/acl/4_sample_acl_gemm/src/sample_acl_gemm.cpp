/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "acl/acl.h"
#include "acl/ops/acl_cblas.h"
#include "acl/acl_op.h"
#include "aclnn/acl_meta.h"
#include "aclnnop/aclnn_matmul.h"
#include "aclnnop/aclnn_mul.h"
#include "aclnnop/aclnn_add.h"

static constexpr int64_t kM = 64;
static constexpr int64_t kN = 64;
static constexpr int64_t kK = 64;
static constexpr int64_t kNumDims = 2;
static constexpr int64_t kNumInputs = 5;
static constexpr float kAlpha = 2.0F;
static constexpr float kBeta = 2.0F;
static constexpr float kOne = 1.0F;
static constexpr uint32_t kSeed = 42U;
static constexpr const char *kModelDir = "../model";
static constexpr const char *kJsonPath = "../model/gemm_fp16.json";
static constexpr mode_t kDirMode = 0750;

static bool CheckAcl(aclError ret, const char *message) {
  if (ret != ACL_SUCCESS) {
    std::cerr << "[ERROR] " << message << " failed, ret=" << ret << std::endl;
    return false;
  }
  return true;
}

static bool CheckAclnn(aclnnStatus ret, const char *message) {
  if (ret != OK) {
    std::cerr << "[ERROR] " << message << " failed, ret=" << ret << std::endl;
    return false;
  }
  return true;
}

static bool EnsureDir(const char *path) {
  if (mkdir(path, kDirMode) != 0 && errno != EEXIST) {
    std::cerr << "[ERROR] create directory failed: " << path << ", errno=" << errno << std::endl;
    return false;
  }
  return true;
}

static bool WriteGemmJson() {
  if (!EnsureDir(kModelDir)) {
    return false;
  }

  std::ofstream json(kJsonPath, std::ios::trunc);
  if (!json.is_open()) {
    std::cerr << "[ERROR] open json failed: " << kJsonPath << std::endl;
    return false;
  }

  json << "[\n"
       << "  {\n"
       << "    \"op\": \"GEMM\",\n"
       << "    \"input_desc\": [\n"
       << "      { \"format\": \"ND\", \"shape\": [64, 64], \"type\": \"float16\" },\n"
       << "      { \"format\": \"ND\", \"shape\": [64, 64], \"type\": \"float16\" },\n"
       << "      { \"format\": \"ND\", \"shape\": [64, 64], \"type\": \"float16\" },\n"
       << "      { \"format\": \"ND\", \"shape\": [], \"type\": \"float16\" },\n"
       << "      { \"format\": \"ND\", \"shape\": [], \"type\": \"float16\" }\n"
       << "    ],\n"
       << "    \"output_desc\": [\n"
       << "      { \"format\": \"ND\", \"shape\": [64, 64], \"type\": \"float16\" }\n"
       << "    ],\n"
       << "    \"attr\": [\n"
       << "      { \"name\": \"transpose_a\", \"type\": \"bool\", \"value\": false },\n"
       << "      { \"name\": \"transpose_b\", \"type\": \"bool\", \"value\": false }\n"
       << "    ]\n"
       << "  }\n"
       << "]\n";
  if (!json.good()) {
    std::cerr << "[ERROR] write json failed: " << kJsonPath << std::endl;
    return false;
  }
  return true;
}

static bool IsValidSocVersion(const std::string &socVersion) {
  if (socVersion.empty()) {
    return false;
  }
  for (const char ch : socVersion) {
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' ||
        ch == '-') {
      continue;
    }
    return false;
  }
  return true;
}

static bool CompileSingleOpOm() {
  if (!WriteGemmJson()) {
    return false;
  }

  const char *socVersion = std::getenv("SOC_VERSION");
  const std::string soc = (socVersion == nullptr || std::strlen(socVersion) == 0) ? "Ascend910B3" : socVersion;
  if (!IsValidSocVersion(soc)) {
    std::cerr << "[ERROR] invalid SOC_VERSION: " << soc << std::endl;
    return false;
  }

  std::stringstream command;
  command << "atc --singleop=" << kJsonPath << " --soc_version=" << soc << " --op_select_implmode=high_precision"
          << " --precision_mode=must_keep_origin_dtype"
          << " --output=" << kModelDir;
  std::cout << "[INFO] " << command.str() << std::endl;

  const int ret = std::system(command.str().c_str());
  if (ret != 0) {
    std::cerr << "[ERROR] atc failed, ret=" << ret << std::endl;
    return false;
  }
  return true;
}

static uint16_t FloatToFp16(float value) {
  return aclFloatToFloat16(value);
}

static void GenerateInputs(std::vector<uint16_t> &a, std::vector<uint16_t> &b, std::vector<uint16_t> &c) {
  srand(kSeed);
  for (auto &value : a) {
    const float fp32 = static_cast<float>(rand() % 2001 - 1000) / 1000.0F;
    value = FloatToFp16(fp32);
  }
  for (auto &value : b) {
    const float fp32 = static_cast<float>(rand() % 2001 - 1000) / 1000.0F;
    value = FloatToFp16(fp32);
  }
  for (auto &value : c) {
    const float fp32 = static_cast<float>(rand() % 2001 - 1000) / 1000.0F;
    value = FloatToFp16(fp32);
  }
}

static void FreeDeviceMem(void *&devPtr) {
  if (devPtr != nullptr) {
    (void)aclrtFree(devPtr);
    devPtr = nullptr;
  }
}

static bool RunAclblasGemm(aclrtStream stream, const std::vector<uint16_t> &hostA, const std::vector<uint16_t> &hostB,
                           const std::vector<uint16_t> &hostC, std::vector<uint16_t> &result) {
  std::cout << "[INFO] Running ACLBLAS GEMM..." << std::endl;

  void *devA = nullptr;
  void *devB = nullptr;
  void *devC = nullptr;
  void *devAlpha = nullptr;
  void *devBeta = nullptr;
  bool ok = true;

  const size_t bytesA = hostA.size() * sizeof(uint16_t);
  const size_t bytesB = hostB.size() * sizeof(uint16_t);
  const size_t bytesC = hostC.size() * sizeof(uint16_t);
  const uint16_t alpha = FloatToFp16(kAlpha);
  const uint16_t beta = FloatToFp16(kBeta);

  do {
    ok = CheckAcl(aclrtMalloc(&devA, bytesA, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devA") &&
         CheckAcl(aclrtMalloc(&devB, bytesB, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devB") &&
         CheckAcl(aclrtMalloc(&devC, bytesC, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devC") &&
         CheckAcl(aclrtMalloc(&devAlpha, sizeof(uint16_t), ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devAlpha") &&
         CheckAcl(aclrtMalloc(&devBeta, sizeof(uint16_t), ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devBeta");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtMemcpy(devA, bytesA, hostA.data(), bytesA, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devA") &&
         CheckAcl(aclrtMemcpy(devB, bytesB, hostB.data(), bytesB, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devB") &&
         CheckAcl(aclrtMemcpy(devC, bytesC, hostC.data(), bytesC, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devC") &&
         CheckAcl(aclrtMemcpy(devAlpha, sizeof(uint16_t), &alpha, sizeof(uint16_t), ACL_MEMCPY_HOST_TO_DEVICE),
                  "aclrtMemcpy devAlpha") &&
         CheckAcl(aclrtMemcpy(devBeta, sizeof(uint16_t), &beta, sizeof(uint16_t), ACL_MEMCPY_HOST_TO_DEVICE),
                  "aclrtMemcpy devBeta");
    if (!ok) {
      break;
    }

    ok =
        CheckAcl(aclblasGemmEx(ACL_TRANS_N, ACL_TRANS_N, ACL_TRANS_N, kM, kN, kK, devAlpha, devA, -1, ACL_FLOAT16, devB,
                               -1, ACL_FLOAT16, devBeta, devC, -1, ACL_FLOAT16, ACL_COMPUTE_HIGH_PRECISION, stream),
                 "aclblasGemmEx");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtSynchronizeStream(stream), "aclrtSynchronizeStream");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtMemcpy(result.data(), bytesC, devC, bytesC, ACL_MEMCPY_DEVICE_TO_HOST), "aclrtMemcpy result");
  } while (0);

  FreeDeviceMem(devA);
  FreeDeviceMem(devB);
  FreeDeviceMem(devC);
  FreeDeviceMem(devAlpha);
  FreeDeviceMem(devBeta);
  return ok;
}

static void DestroyDataBuffer(aclDataBuffer *&buffer) {
  if (buffer != nullptr) {
    (void)aclDestroyDataBuffer(buffer);
    buffer = nullptr;
  }
}

static void DestroyTensorDesc(aclTensorDesc *&desc) {
  if (desc != nullptr) {
    (void)aclDestroyTensorDesc(desc);
    desc = nullptr;
  }
}

static bool RunAclopGemm(aclrtStream stream, const std::vector<uint16_t> &hostA, const std::vector<uint16_t> &hostB,
                         const std::vector<uint16_t> &hostC, std::vector<uint16_t> &result) {
  std::cout << "[INFO] Running ACLOP GEMM..." << std::endl;

  void *devA = nullptr;
  void *devB = nullptr;
  void *devC = nullptr;
  void *devAlpha = nullptr;
  void *devBeta = nullptr;
  aclTensorDesc *descA = nullptr;
  aclTensorDesc *descB = nullptr;
  aclTensorDesc *descC = nullptr;
  aclTensorDesc *descAlpha = nullptr;
  aclTensorDesc *descBeta = nullptr;
  aclDataBuffer *bufA = nullptr;
  aclDataBuffer *bufB = nullptr;
  aclDataBuffer *bufC = nullptr;
  aclDataBuffer *bufAlpha = nullptr;
  aclDataBuffer *bufBeta = nullptr;
  aclopAttr *attr = nullptr;
  bool ok = true;

  const size_t bytesA = hostA.size() * sizeof(uint16_t);
  const size_t bytesB = hostB.size() * sizeof(uint16_t);
  const size_t bytesC = hostC.size() * sizeof(uint16_t);
  const uint16_t alpha = FloatToFp16(kAlpha);
  const uint16_t beta = FloatToFp16(kBeta);
  int64_t shapeA[2] = {kM, kK};
  int64_t shapeB[2] = {kK, kN};
  int64_t shapeC[2] = {kM, kN};

  do {
    ok = CheckAcl(aclrtMalloc(&devA, bytesA, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devA") &&
         CheckAcl(aclrtMalloc(&devB, bytesB, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devB") &&
         CheckAcl(aclrtMalloc(&devC, bytesC, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devC") &&
         CheckAcl(aclrtMalloc(&devAlpha, sizeof(uint16_t), ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devAlpha") &&
         CheckAcl(aclrtMalloc(&devBeta, sizeof(uint16_t), ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devBeta");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtMemcpy(devA, bytesA, hostA.data(), bytesA, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devA") &&
         CheckAcl(aclrtMemcpy(devB, bytesB, hostB.data(), bytesB, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devB") &&
         CheckAcl(aclrtMemcpy(devC, bytesC, hostC.data(), bytesC, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devC") &&
         CheckAcl(aclrtMemcpy(devAlpha, sizeof(uint16_t), &alpha, sizeof(uint16_t), ACL_MEMCPY_HOST_TO_DEVICE),
                  "aclrtMemcpy devAlpha") &&
         CheckAcl(aclrtMemcpy(devBeta, sizeof(uint16_t), &beta, sizeof(uint16_t), ACL_MEMCPY_HOST_TO_DEVICE),
                  "aclrtMemcpy devBeta");
    if (!ok) {
      break;
    }

    descA = aclCreateTensorDesc(ACL_FLOAT16, kNumDims, shapeA, ACL_FORMAT_ND);
    descB = aclCreateTensorDesc(ACL_FLOAT16, kNumDims, shapeB, ACL_FORMAT_ND);
    descC = aclCreateTensorDesc(ACL_FLOAT16, kNumDims, shapeC, ACL_FORMAT_ND);
    descAlpha = aclCreateTensorDesc(ACL_FLOAT16, 0, nullptr, ACL_FORMAT_ND);
    descBeta = aclCreateTensorDesc(ACL_FLOAT16, 0, nullptr, ACL_FORMAT_ND);
    if (descA == nullptr || descB == nullptr || descC == nullptr || descAlpha == nullptr || descBeta == nullptr) {
      std::cerr << "[ERROR] create tensor desc failed" << std::endl;
      ok = false;
      break;
    }

    bufA = aclCreateDataBuffer(devA, bytesA);
    bufB = aclCreateDataBuffer(devB, bytesB);
    bufC = aclCreateDataBuffer(devC, bytesC);
    bufAlpha = aclCreateDataBuffer(devAlpha, sizeof(uint16_t));
    bufBeta = aclCreateDataBuffer(devBeta, sizeof(uint16_t));
    if (bufA == nullptr || bufB == nullptr || bufC == nullptr || bufAlpha == nullptr || bufBeta == nullptr) {
      std::cerr << "[ERROR] create data buffer failed" << std::endl;
      ok = false;
      break;
    }

    attr = aclopCreateAttr();
    if (attr == nullptr) {
      std::cerr << "[ERROR] create aclop attr failed" << std::endl;
      ok = false;
      break;
    }
    ok = CheckAcl(aclopSetAttrBool(attr, "transpose_a", false), "aclopSetAttrBool transpose_a") &&
         CheckAcl(aclopSetAttrBool(attr, "transpose_b", false), "aclopSetAttrBool transpose_b");
    if (!ok) {
      break;
    }

    aclTensorDesc *inputDescs[5] = {descA, descB, descC, descAlpha, descBeta};
    aclDataBuffer *inputs[5] = {bufA, bufB, bufC, bufAlpha, bufBeta};
    aclTensorDesc *outputDescs[1] = {descC};
    aclDataBuffer *outputs[1] = {bufC};
    ok = CheckAcl(aclopExecuteV2("GEMM", kNumInputs, inputDescs, inputs, 1, outputDescs, outputs, attr, stream),
                  "aclopExecuteV2 GEMM");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtSynchronizeStream(stream), "aclrtSynchronizeStream");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtMemcpy(result.data(), bytesC, devC, bytesC, ACL_MEMCPY_DEVICE_TO_HOST), "aclrtMemcpy result");
  } while (0);

  if (attr != nullptr) {
    (void)aclopDestroyAttr(attr);
  }
  DestroyDataBuffer(bufA);
  DestroyDataBuffer(bufB);
  DestroyDataBuffer(bufC);
  DestroyDataBuffer(bufAlpha);
  DestroyDataBuffer(bufBeta);
  DestroyTensorDesc(descA);
  DestroyTensorDesc(descB);
  DestroyTensorDesc(descC);
  DestroyTensorDesc(descAlpha);
  DestroyTensorDesc(descBeta);
  FreeDeviceMem(devA);
  FreeDeviceMem(devB);
  FreeDeviceMem(devC);
  FreeDeviceMem(devAlpha);
  FreeDeviceMem(devBeta);
  return ok;
}

static void DestroyTensor(aclTensor *&tensor) {
  if (tensor != nullptr) {
    (void)aclDestroyTensor(tensor);
    tensor = nullptr;
  }
}

static void DestroyScalar(aclScalar *&scalar) {
  if (scalar != nullptr) {
    (void)aclDestroyScalar(scalar);
    scalar = nullptr;
  }
}

static bool RunAclnnGemm(aclrtStream stream, const std::vector<uint16_t> &hostA, const std::vector<uint16_t> &hostB,
                         const std::vector<uint16_t> &hostC, std::vector<uint16_t> &result) {
  std::cout << "[INFO] Running ACLNN GEMM..." << std::endl;

  void *devA = nullptr;
  void *devB = nullptr;
  void *devC = nullptr;
  void *devOut = nullptr;
  void *devMulAlpha = nullptr;
  void *devMulBeta = nullptr;
  void *workspace = nullptr;
  aclTensor *tensorA = nullptr;
  aclTensor *tensorB = nullptr;
  aclTensor *tensorC = nullptr;
  aclTensor *tensorOut = nullptr;
  aclTensor *tensorMulAlpha = nullptr;
  aclTensor *tensorMulBeta = nullptr;
  aclScalar *scalarAlpha = nullptr;
  aclScalar *scalarBeta = nullptr;
  aclScalar *scalarOne = nullptr;
  aclOpExecutor *execMatmul = nullptr;
  aclOpExecutor *execMulsAlpha = nullptr;
  aclOpExecutor *execMulsBeta = nullptr;
  aclOpExecutor *execAdd = nullptr;
  bool ok = true;

  const size_t bytesA = hostA.size() * sizeof(uint16_t);
  const size_t bytesB = hostB.size() * sizeof(uint16_t);
  const size_t bytesC = hostC.size() * sizeof(uint16_t);
  int64_t shapeA[2] = {kM, kK};
  int64_t shapeB[2] = {kK, kN};
  int64_t shapeC[2] = {kM, kN};
  int64_t stride[2] = {64, 1};
  float alpha = kAlpha;
  float beta = kBeta;
  float one = kOne;
  uint64_t wsMatmul = 0;
  uint64_t wsMulsAlpha = 0;
  uint64_t wsMulsBeta = 0;
  uint64_t wsAdd = 0;
  uint64_t maxWorkspace = 0;

  do {
    ok = CheckAcl(aclrtMalloc(&devA, bytesA, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devA") &&
         CheckAcl(aclrtMalloc(&devB, bytesB, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devB") &&
         CheckAcl(aclrtMalloc(&devC, bytesC, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devC") &&
         CheckAcl(aclrtMalloc(&devOut, bytesC, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devOut") &&
         CheckAcl(aclrtMalloc(&devMulAlpha, bytesC, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devMulAlpha") &&
         CheckAcl(aclrtMalloc(&devMulBeta, bytesC, ACL_MEM_MALLOC_NORMAL_ONLY), "aclrtMalloc devMulBeta");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtMemcpy(devA, bytesA, hostA.data(), bytesA, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devA") &&
         CheckAcl(aclrtMemcpy(devB, bytesB, hostB.data(), bytesB, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devB") &&
         CheckAcl(aclrtMemcpy(devC, bytesC, hostC.data(), bytesC, ACL_MEMCPY_HOST_TO_DEVICE), "aclrtMemcpy devC");
    if (!ok) {
      break;
    }

    tensorA = aclCreateTensor(shapeA, kNumDims, ACL_FLOAT16, stride, 0, ACL_FORMAT_ND, shapeA, kNumDims, devA);
    tensorB = aclCreateTensor(shapeB, kNumDims, ACL_FLOAT16, stride, 0, ACL_FORMAT_ND, shapeB, kNumDims, devB);
    tensorC = aclCreateTensor(shapeC, kNumDims, ACL_FLOAT16, stride, 0, ACL_FORMAT_ND, shapeC, kNumDims, devC);
    tensorOut = aclCreateTensor(shapeC, kNumDims, ACL_FLOAT16, stride, 0, ACL_FORMAT_ND, shapeC, kNumDims, devOut);
    tensorMulAlpha =
        aclCreateTensor(shapeC, kNumDims, ACL_FLOAT16, stride, 0, ACL_FORMAT_ND, shapeC, kNumDims, devMulAlpha);
    tensorMulBeta =
        aclCreateTensor(shapeC, kNumDims, ACL_FLOAT16, stride, 0, ACL_FORMAT_ND, shapeC, kNumDims, devMulBeta);
    if (tensorA == nullptr || tensorB == nullptr || tensorC == nullptr || tensorOut == nullptr ||
        tensorMulAlpha == nullptr || tensorMulBeta == nullptr) {
      std::cerr << "[ERROR] aclCreateTensor failed" << std::endl;
      ok = false;
      break;
    }

    scalarAlpha = aclCreateScalar(&alpha, ACL_FLOAT);
    scalarBeta = aclCreateScalar(&beta, ACL_FLOAT);
    scalarOne = aclCreateScalar(&one, ACL_FLOAT);
    if (scalarAlpha == nullptr || scalarBeta == nullptr || scalarOne == nullptr) {
      std::cerr << "[ERROR] aclCreateScalar failed" << std::endl;
      ok = false;
      break;
    }

    ok = CheckAclnn(aclnnMatmulGetWorkspaceSize(tensorA, tensorB, tensorOut, 0, &wsMatmul, &execMatmul),
                    "aclnnMatmulGetWorkspaceSize") &&
         CheckAclnn(aclnnMulsGetWorkspaceSize(tensorOut, scalarAlpha, tensorMulAlpha, &wsMulsAlpha, &execMulsAlpha),
                    "aclnnMulsGetWorkspaceSize alpha") &&
         CheckAclnn(aclnnMulsGetWorkspaceSize(tensorC, scalarBeta, tensorMulBeta, &wsMulsBeta, &execMulsBeta),
                    "aclnnMulsGetWorkspaceSize beta") &&
         CheckAclnn(aclnnAddGetWorkspaceSize(tensorMulAlpha, tensorMulBeta, scalarOne, tensorOut, &wsAdd, &execAdd),
                    "aclnnAddGetWorkspaceSize");
    if (!ok) {
      break;
    }

    maxWorkspace = std::max(std::max(wsMatmul, wsMulsAlpha), std::max(wsMulsBeta, wsAdd));
    if (maxWorkspace > 0) {
      ok = CheckAcl(aclrtMalloc(&workspace, maxWorkspace, ACL_MEM_MALLOC_HUGE_FIRST), "aclrtMalloc workspace");
      if (!ok) {
        break;
      }
    }

    ok = CheckAclnn(aclnnMatmul(workspace, wsMatmul, execMatmul, stream), "aclnnMatmul") &&
         CheckAclnn(aclnnMuls(workspace, wsMulsAlpha, execMulsAlpha, stream), "aclnnMuls alpha") &&
         CheckAclnn(aclnnMuls(workspace, wsMulsBeta, execMulsBeta, stream), "aclnnMuls beta") &&
         CheckAclnn(aclnnAdd(workspace, wsAdd, execAdd, stream), "aclnnAdd");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtSynchronizeStream(stream), "aclrtSynchronizeStream");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclrtMemcpy(result.data(), bytesC, devOut, bytesC, ACL_MEMCPY_DEVICE_TO_HOST), "aclrtMemcpy result");
  } while (0);

  DestroyScalar(scalarAlpha);
  DestroyScalar(scalarBeta);
  DestroyScalar(scalarOne);
  DestroyTensor(tensorA);
  DestroyTensor(tensorB);
  DestroyTensor(tensorC);
  DestroyTensor(tensorOut);
  DestroyTensor(tensorMulAlpha);
  DestroyTensor(tensorMulBeta);
  FreeDeviceMem(workspace);
  FreeDeviceMem(devA);
  FreeDeviceMem(devB);
  FreeDeviceMem(devC);
  FreeDeviceMem(devOut);
  FreeDeviceMem(devMulAlpha);
  FreeDeviceMem(devMulBeta);
  return ok;
}

static bool CompareResults(const std::vector<uint16_t> &r1, const std::vector<uint16_t> &r2,
                           const std::vector<uint16_t> &r3, float &max_error) {
  std::cout << "[INFO] Comparing results..." << std::endl;

  max_error = 0.0f;
  for (size_t i = 0; i < r1.size(); ++i) {
    const float v1 = aclFloat16ToFloat(r1[i]);
    const float v2 = aclFloat16ToFloat(r2[i]);
    const float v3 = aclFloat16ToFloat(r3[i]);

    const float err12 = std::abs(v1 - v2);
    const float err13 = std::abs(v1 - v3);
    const float err23 = std::abs(v2 - v3);

    max_error = std::max(max_error, std::max(err12, std::max(err13, err23)));
  }

  return max_error == 0.0f;
}

int main() {
  std::cout << "[INFO] ACL GEMM sample starts" << std::endl;

  if (!CompileSingleOpOm()) {
    return 1;
  }

  bool aclInitialized = false;
  bool deviceSet = false;
  aclrtStream stream = nullptr;
  bool ok = true;

  do {
    ok = CheckAcl(aclInit(nullptr), "aclInit");
    if (!ok) {
      break;
    }
    aclInitialized = true;

    ok = CheckAcl(aclrtSetDevice(0), "aclrtSetDevice");
    if (!ok) {
      break;
    }
    deviceSet = true;

    ok = CheckAcl(aclrtCreateStream(&stream), "aclrtCreateStream");
    if (!ok) {
      break;
    }

    ok = CheckAcl(aclopSetModelDir(kModelDir), "aclopSetModelDir");
    if (!ok) {
      break;
    }

    std::vector<uint16_t> hostA(kM * kK);
    std::vector<uint16_t> hostB(kK * kN);
    std::vector<uint16_t> hostC(kM * kN);
    GenerateInputs(hostA, hostB, hostC);

    std::vector<uint16_t> resultAclblas(kM * kN);
    std::vector<uint16_t> resultAclop(kM * kN);
    std::vector<uint16_t> resultAclnn(kM * kN);

    ok = RunAclblasGemm(stream, hostA, hostB, hostC, resultAclblas);
    if (!ok) {
      std::cerr << "[ERROR] ACLBLAS GEMM failed" << std::endl;
      break;
    }

    ok = RunAclopGemm(stream, hostA, hostB, hostC, resultAclop);
    if (!ok) {
      std::cerr << "[ERROR] ACLOP GEMM failed" << std::endl;
      break;
    }

    ok = RunAclnnGemm(stream, hostA, hostB, hostC, resultAclnn);
    if (!ok) {
      std::cerr << "[ERROR] ACLNN GEMM failed" << std::endl;
      break;
    }

    float max_error = 0.0f;
    ok = CompareResults(resultAclblas, resultAclop, resultAclnn, max_error);
    std::cout << "max_error: " << max_error << std::endl;
    std::cout << (ok ? "[INFO] VERIFICATION PASSED" : "[ERROR] VERIFICATION FAILED") << std::endl;
  } while (0);

  if (stream != nullptr) {
    (void)aclrtDestroyStream(stream);
  }
  if (deviceSet) {
    (void)aclrtResetDevice(0);
  }
  if (aclInitialized) {
    (void)aclFinalize();
  }

  std::cout << (ok ? "[INFO] SAMPLE PASSED" : "[ERROR] SAMPLE FAILED") << std::endl;
  return ok ? 0 : 1;
}
