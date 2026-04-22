/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdint>
#include <memory>
#include <stdexcept>

#include "bindings.h"
#include "ge/fusion/pattern_matcher_config.h"

namespace ge {
namespace python_pass_native {
namespace {
using ::ge::fusion::PatternMatcherConfig;
using ::ge::fusion::PatternMatcherConfigBuilder;

class NativePatternMatcherConfig {
 public:
  explicit NativePatternMatcherConfig(std::shared_ptr<PatternMatcherConfig> config) : config_(std::move(config)) {}

  bool IsEnableConstValueMatch() const {
    return (config_ != nullptr) && config_->IsEnableConstValueMatch();
  }

  bool IsEnableIrAttrMatch() const {
    return (config_ != nullptr) && config_->IsEnableIrAttrMatch();
  }

  std::unique_ptr<PatternMatcherConfig> Clone() const {
    if (config_ == nullptr) {
      return nullptr;
    }
    // make失败should throw exception, python会捕获异常
    return std::make_unique<PatternMatcherConfig>(*config_);
  }

 private:
  std::shared_ptr<PatternMatcherConfig> config_;
};

class NativePatternMatcherConfigBuilder {
 public:
  NativePatternMatcherConfigBuilder &EnableConstValueMatch() {
    builder_.EnableConstValueMatch();
    return *this;
  }

  NativePatternMatcherConfigBuilder &EnableIrAttrMatch() {
    builder_.EnableIrAttrMatch();
    return *this;
  }

  NativePatternMatcherConfig Build() const {
    auto config = builder_.Build();
    if (config == nullptr) {
      throw std::runtime_error("Failed to build PatternMatcherConfig");
    }
    return NativePatternMatcherConfig(std::shared_ptr<PatternMatcherConfig>(std::move(config)));
  }

 private:
  PatternMatcherConfigBuilder builder_;
};

uintptr_t ClonePatternMatcherConfigHandle(const NativePatternMatcherConfig &config) {
  auto config_copy = config.Clone();
  return reinterpret_cast<uintptr_t>(config_copy.release());
}
}  // namespace

void BindPatternMatcherConfig(py::module_ &m) {
  py::class_<NativePatternMatcherConfig>(m, "PatternMatcherConfig")
      .def("is_enable_const_value_match", &NativePatternMatcherConfig::IsEnableConstValueMatch)
      .def("is_enable_ir_attr_match", &NativePatternMatcherConfig::IsEnableIrAttrMatch);

  py::class_<NativePatternMatcherConfigBuilder>(m, "PatternMatcherConfigBuilder")
      .def(py::init<>())
      .def("enable_const_value_match", &NativePatternMatcherConfigBuilder::EnableConstValueMatch,
           py::return_value_policy::reference_internal)
      .def("enable_ir_attr_match", &NativePatternMatcherConfigBuilder::EnableIrAttrMatch,
           py::return_value_policy::reference_internal)
      .def("build", &NativePatternMatcherConfigBuilder::Build);

  m.def("clone_pattern_matcher_config", &ClonePatternMatcherConfigHandle, py::arg("config"));
}

}  // namespace python_pass_native
}  // namespace ge
