/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "run_bundle_model.h"
#include "../common.h"
#include "acl/acl.h"
#include "acl/acl_mdl.h"
#include "acl/acl_rt.h"
#include <iostream>
#include <vector>

namespace {

ge::Status LoadBundleModelIds(uint32_t bundle_id, std::vector<uint32_t> *model_ids) {
  size_t model_num = 0;
  aclError err = aclmdlBundleGetModelNum(bundle_id, &model_num);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlBundleGetModelNum 失败, aclError=" << err << std::endl;
    return ge::FAILED;
  }
  model_ids->resize(model_num);
  for (size_t i = 0; i < model_num; ++i) {
    err = aclmdlBundleGetModelId(bundle_id, i, &(*model_ids)[i]);
    if (err != ACL_SUCCESS) {
      std::cerr << "[Error] aclmdlBundleGetModelId 失败, aclError=" << err << std::endl;
      return ge::FAILED;
    }
  }
  return ge::SUCCESS;
}

ge::Status RunBundleSubmodelInfer(const uint32_t model_id, const char *label,
                                  const std::vector<std::vector<float>> &host_inputs) {
  std::cout << "[Info] 执行子模型推理，model_id: " << model_id << "\n";
  aclmdlDesc *model_desc = aclmdlCreateDesc();
  if (model_desc == nullptr) {
    std::cerr << "[Error] aclmdlCreateDesc 失败\n";
    return ge::FAILED;
  }
  aclError e = aclmdlGetDesc(model_desc, model_id);
  if (e != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlGetDesc 失败, aclError=" << e << std::endl;
    TeardownBundleSubmodelSession(model_desc, nullptr, nullptr);
    return ge::FAILED;
  }
  IoBuffers input_io;
  IoBuffers output_io;
  if (CreateIoDataset(model_desc, true, &input_io) != ge::SUCCESS) {
    TeardownBundleSubmodelSession(model_desc, nullptr, nullptr);
    return ge::FAILED;
  }
  if (CreateIoDataset(model_desc, false, &output_io) != ge::SUCCESS) {
    TeardownBundleSubmodelSession(model_desc, &input_io, nullptr);
    return ge::FAILED;
  }
  if (CopyFloatInputs(host_inputs, input_io) != ge::SUCCESS) {
    TeardownBundleSubmodelSession(model_desc, &input_io, &output_io);
    return ge::FAILED;
  }
  e = aclmdlExecute(model_id, input_io.dataset, output_io.dataset);
  if (e != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlExecute 失败, aclError=" << e << std::endl;
    TeardownBundleSubmodelSession(model_desc, &input_io, &output_io);
    return ge::FAILED;
  }
  std::cout << "[Info] " << label << " 推理结果:\n";
  if (PrintModelOutputs(model_desc, output_io) != ge::SUCCESS) {
    TeardownBundleSubmodelSession(model_desc, &input_io, &output_io);
    return ge::FAILED;
  }
  TeardownBundleSubmodelSession(model_desc, &input_io, &output_io);
  return ge::SUCCESS;
}
}  // namespace

int RunBundleModelInfer() {
  std::cout << "[Info] 初始化运行环境及数据\n";
  aclError err = aclInit(nullptr);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclInit 失败, aclError=" << err << std::endl;
    return -1;
  }
  err = aclrtSetDevice(kDeviceId);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclrtSetDevice 失败, aclError=" << err << std::endl;
    (void)aclFinalize();
    return -1;
  }

  uint32_t bundle_id = 0;
  err = aclmdlBundleLoadFromFile("bundle_sample.om", &bundle_id);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlBundleLoadFromFile 失败, aclError=" << err << std::endl;
    (void)aclrtResetDevice(kDeviceId);
    (void)aclFinalize();
    return -1;
  }

  std::vector<uint32_t> model_ids;
  if (LoadBundleModelIds(bundle_id, &model_ids) != ge::SUCCESS) {
    TeardownAclBundleInfer(bundle_id);
    return -1;
  }
  std::cout << "[Info] Bundle 模型加载成功，子模型个数: " << model_ids.size() << "\n";

  const auto inputs = SampleInputs();
  if (RunBundleSubmodelInfer(model_ids[0], "Add 子模型", inputs) != ge::SUCCESS) {
    TeardownAclBundleInfer(bundle_id);
    return -1;
  }
  if (RunBundleSubmodelInfer(model_ids[1], "Mul 子模型", inputs) != ge::SUCCESS) {
    TeardownAclBundleInfer(bundle_id);
    return -1;
  }

  TeardownAclBundleInfer(bundle_id);
  std::cout << "[Info] 所有资源释放成功\n";
  return 0;
}
