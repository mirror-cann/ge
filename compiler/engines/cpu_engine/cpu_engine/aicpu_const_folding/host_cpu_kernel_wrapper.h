/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#ifndef GE_GE_LOCAL_ENGINE_ENGINE_HOST_CPU_KERNEL_WRAPPER_H_
#define GE_GE_LOCAL_ENGINE_ENGINE_HOST_CPU_KERNEL_WRAPPER_H_

#include "register/graph_register.h"

namespace ge {
class HostCpuKernelWrapperOpV2 : public HostCpuOp {
 public:
  HostCpuKernelWrapperOpV2();
  ~HostCpuKernelWrapperOpV2() override = default;
  graphStatus Compute(Operator &op, const std::map<std::string, const ge::Tensor> &inputs,
                      std::map<std::string, ge::Tensor> &outputs) override;
};
}  // namespace ge
#endif  // GE_GE_LOCAL_ENGINE_ENGINE_HOST_CPU_KERNEL_WRAPPER_H_
