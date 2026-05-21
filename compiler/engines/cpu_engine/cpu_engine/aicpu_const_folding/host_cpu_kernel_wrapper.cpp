/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "host_cpu_kernel_wrapper.h"
#include <unistd.h>
#include <string>
#include "folding.h"
#include "register/host_cpu_context.h"
#include "graph/utils/op_desc_utils.h"
#include "util/log.h"

extern "C" {
  FMK_FUNC_HOST_VISIBILITY ge::graphStatus Initialize(const ge::HostCpuContext &ctx) {
  (void)ctx;
  AICPUE_LOGI("Enter host CPU kernel wrapper initialize");
  int32_t ret = InitCpuConstantFoldingNew([]() -> ge::HostCpuOp* {
    return new (std::nothrow) ge::HostCpuKernelWrapperOpV2();
  });
  if (ret != 0) {
    AICPUE_LOGW("InitCpuConstantFoldingNew run failed, ret: %d", ret);
    return ge::GRAPH_FAILED;
  }

  AICPUE_LOGI("Host CPU kernel wrapper initialize successfully");
  return ge::GRAPH_SUCCESS;
}
}

namespace ge {
HostCpuKernelWrapperOpV2::HostCpuKernelWrapperOpV2() = default;

graphStatus HostCpuKernelWrapperOpV2::Compute(
    ge::Operator &op,
    const std::map<std::string, const ge::Tensor> &inputs,
    std::map<std::string, ge::Tensor> &outputs) {
  AICPUE_LOGI("Enter host CPU kernel wrapper compute");

  int32_t ret = CpuConstantFoldingComputeNew(op, inputs, outputs);
  if (ret != 0) {
    AICPUE_LOGW("CpuConstantFoldingComputeNew run failed, ret: %d", ret);
    return ge::GRAPH_FAILED;
  }

  AICPUE_LOGI("Host CPU kernel wrapper compute successfully");
  return ge::GRAPH_SUCCESS;
}

REGISTER_HOST_CPU_OP_BUILDER("HostCpuKernelWrapperOpV2", ge::HostCpuKernelWrapperOpV2);
}