/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <string>
#include <vector>

#include "main_impl.h"

extern "C" int GeApiWrapper_Atc_Main(int argc, const char *const *argv) {
  if ((argc <= 0) || (argv == nullptr)) {
    return 1;
  }
  std::vector<std::string> owned_args;
  std::vector<char *> mutable_argv;
  owned_args.reserve(static_cast<size_t>(argc));
  mutable_argv.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    if (argv[i] == nullptr) {
      return 1;
    }
    owned_args.emplace_back(argv[i]);
  }
  for (auto &arg : owned_args) {
    mutable_argv.push_back(arg.data());
  }
  return ge::main_impl(argc, mutable_argv.data());
}
