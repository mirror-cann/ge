/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "src/single_model/build_add_model.h"
#include "src/single_model/run_add_model.h"
#include "src/bundle_model/build_bundle_model.h"
#include "src/bundle_model/run_bundle_model.h"
#include <cstring>
#include <iostream>
#include <string>

namespace {
constexpr int kArgIndexAfterCommand = 2;

bool ParseSocVersion(int argc, char **argv, int start_index, std::string *soc_version) {
  for (int i = start_index; i < argc; ++i) {
    if (std::strcmp(argv[i], "--soc-version") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "[Error] --soc-version 参数为空\n";
        return false;
      }
      *soc_version = argv[i + 1];
      ++i;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[Error] 用法: offline_compile_sample <命令> [--soc-version <版本>]\n"
              << "  命令: build-model | build-bundle | run-infer | run-bundle-infer\n";
    return -1;
  }
  const std::string cmd = argv[1];
  std::string soc_version;
  if (!ParseSocVersion(argc, argv, kArgIndexAfterCommand, &soc_version)) {
    return -1;
  }
  if (cmd == "build-model") {
    return BuildAddModel(soc_version);
  }
  if (cmd == "build-bundle") {
    return BuildBundleModel(soc_version);
  }
  if (cmd == "run-infer") {
    return RunSingleModelInfer();
  }
  if (cmd == "run-bundle-infer") {
    return RunBundleModelInfer();
  }
  std::cerr << "[Error] 未知命令: " << cmd << std::endl;
  return -1;
}
