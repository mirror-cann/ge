/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef EXAMPLES_OFFLINE_COMPILE_RUN_CPP_SRC_COMMON_H_
#define EXAMPLES_OFFLINE_COMPILE_RUN_CPP_SRC_COMMON_H_

#include "acl/acl.h"
#include "acl/acl_mdl.h"
#include "ge/ge_api_error_codes.h"
#include "graph/graph.h"
#include <memory>
#include <vector>

constexpr int32_t kDeviceId = 0;

struct IoBuffers {
  aclmdlDataset *dataset = nullptr;
  std::vector<void *> device_ptrs;
  std::vector<size_t> sizes;
};

ge::Status CreateIoDataset(aclmdlDesc *model_desc, bool is_input, IoBuffers *out);
ge::Status CopyFloatInputs(const std::vector<std::vector<float>> &host_inputs, const IoBuffers &inputs);
ge::Status PrintInferOutputDataset(aclmdlDesc *model_desc, aclmdlDataset *output_dataset);
ge::Status PrintModelOutputs(aclmdlDesc *model_desc, const IoBuffers &outputs);
std::vector<std::vector<float>> SampleInputs();
std::unique_ptr<ge::Graph> MakeOfflineAddGraph();
std::unique_ptr<ge::Graph> MakeOfflineMulGraph();
void ReleaseDataset(aclmdlDataset *dataset);
void TeardownAclSingleModelInfer(IoBuffers *input_io, IoBuffers *output_io, aclmdlDesc *model_desc, uint32_t model_id);
void TeardownAclBundleInfer(uint32_t bundle_id);
void TeardownBundleSubmodelSession(aclmdlDesc *model_desc, IoBuffers *input_io, IoBuffers *output_io);

#endif  // EXAMPLES_OFFLINE_COMPILE_RUN_CPP_SRC_COMMON_H_
