/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_REGISTER_AMCT_REGISTRY_H_
#define INC_REGISTER_AMCT_REGISTRY_H_

#include "amct_interface.h"
#include <memory>

namespace ge {
using IAmctCalibrationPtr = std::shared_ptr<IAmctCalibration>;

class GeAmctRegistry {
 public:
  ~GeAmctRegistry() = default;

  GeAmctRegistry(const GeAmctRegistry &) = delete;
  GeAmctRegistry(GeAmctRegistry &&) = delete;
  GeAmctRegistry &operator=(const GeAmctRegistry &) = delete;
  GeAmctRegistry &operator=(GeAmctRegistry &&) = delete;

  static GeAmctRegistry &GetInstance();

  void Register(const IAmctCalibrationPtr &calibration);

  graphStatus Invoke(Graph &graph, const std::map<std::string, std::string> &options) const;

  void Unregister();

 private:
  GeAmctRegistry() = default;

  IAmctCalibrationPtr calibration_;
};

class GeAmctRegistrar {
 public:
  explicit GeAmctRegistrar(const IAmctCalibrationPtr &calibration);
};

}  // namespace ge

#define REG_AMCT_CALIBRATION(class_name) REG_AMCT_CALIBRATION_UNIQ_(__COUNTER__, class_name)
#define REG_AMCT_CALIBRATION_UNIQ_(ctr, class_name) REG_AMCT_CALIBRATION_UNIQ(ctr, class_name)
#define REG_AMCT_CALIBRATION_UNIQ(ctr, class_name)                            \
  static __attribute__((unused)) ge::GeAmctRegistrar ge_amct_registrar##ctr = \
      ge::GeAmctRegistrar(std::make_shared<class_name>())

#endif  // INC_REGISTER_AMCT_REGISTRY_H_
