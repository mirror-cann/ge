/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_AMCT_INTERFACE_H_
#define INC_EXTERNAL_AMCT_INTERFACE_H_

#include "graph/graph.h"
#include <map>

namespace ge {
class IAmctCalibration {
public:
  virtual ~IAmctCalibration() = default;

  virtual graphStatus Calibrate(Graph& graph, const std::map<std::string, std::string>& options) = 0;
};
} // namespace ge

#endif // INC_EXTERNAL_AMCT_INTERFACE_H_
