/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "run_add_model.h"
#include "../common.h"
#include "acl/acl.h"
#include "acl/acl_mdl.h"
#include "acl/acl_rt.h"
#include <iostream>

namespace {

int ExecuteSingleLoadedModel(aclmdlDesc *model_desc, uint32_t model_id) {
  IoBuffers input_io;
  IoBuffers output_io;
  if (CreateIoDataset(model_desc, true, &input_io) != ge::SUCCESS ||
      CreateIoDataset(model_desc, false, &output_io) != ge::SUCCESS) {
    TeardownAclSingleModelInfer(&input_io, &output_io, model_desc, model_id);
    return -1;
  }
  std::cout << "[Info] 执行模型推理\n";
  if (CopyFloatInputs(SampleInputs(), input_io) != ge::SUCCESS) {
    TeardownAclSingleModelInfer(&input_io, &output_io, model_desc, model_id);
    return -1;
  }
  const aclError err = aclmdlExecute(model_id, input_io.dataset, output_io.dataset);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlExecute 失败, aclError=" << err << std::endl;
    TeardownAclSingleModelInfer(&input_io, &output_io, model_desc, model_id);
    return -1;
  }
  std::cout << "[Info] 模型推理结果:\n";
  if (PrintModelOutputs(model_desc, output_io) != ge::SUCCESS) {
    TeardownAclSingleModelInfer(&input_io, &output_io, model_desc, model_id);
    return -1;
  }
  TeardownAclSingleModelInfer(&input_io, &output_io, model_desc, model_id);
  std::cout << "[Info] 所有资源释放成功\n";
  return 0;
}

}  // namespace

int RunSingleModelInfer() {
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
  uint32_t model_id = 0;
  err = aclmdlLoadFromFile("add_sample.om", &model_id);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlLoadFromFile 失败, aclError=" << err << std::endl;
    (void)aclrtResetDevice(kDeviceId);
    (void)aclFinalize();
    return -1;
  }
  aclmdlDesc *model_desc = aclmdlCreateDesc();
  if (model_desc == nullptr) {
    std::cerr << "[Error] aclmdlCreateDesc 失败\n";
    TeardownAclSingleModelInfer(nullptr, nullptr, nullptr, model_id);
    return -1;
  }
  err = aclmdlGetDesc(model_desc, model_id);
  if (err != ACL_SUCCESS) {
    std::cerr << "[Error] aclmdlGetDesc 失败, aclError=" << err << std::endl;
    TeardownAclSingleModelInfer(nullptr, nullptr, model_desc, model_id);
    return -1;
  }
  return ExecuteSingleLoadedModel(model_desc, model_id);
}
