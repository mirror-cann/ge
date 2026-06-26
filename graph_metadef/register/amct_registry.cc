/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "register/amct_registry.h"
#include "register/amct_interface.h"
#include "debug/ge_log.h"

namespace ge {
GeAmctRegistry &GeAmctRegistry::GetInstance() {
  static GeAmctRegistry instance;
  return instance;
}

void GeAmctRegistry::Register(const IAmctCalibrationPtr &calibration) {
  if (calibration_ == nullptr) {
    calibration_ = calibration;
    GELOGI("IAmctCalibration register success");
  } else {
    GELOGW("IAmctCalibration is already registered");
  }
}

graphStatus GeAmctRegistry::Invoke(Graph &graph, const std::map<std::string, std::string> &options) const {
  if (!calibration_) {
    GELOGW("IAmctCalibration instance is not registered, cannot invoke");
    return GRAPH_PARAM_INVALID;
  }
  return calibration_->Calibrate(graph, options);
}

void GeAmctRegistry::Unregister() {
  if (calibration_ != nullptr) {
    calibration_.reset();
  }
}

GeAmctRegistrar::GeAmctRegistrar(const IAmctCalibrationPtr &calibration) {
  GeAmctRegistry::GetInstance().Register(calibration);
}
}  // namespace ge
