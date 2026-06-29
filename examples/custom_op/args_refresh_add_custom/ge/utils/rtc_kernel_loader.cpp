/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "rtc_kernel_loader.h"
#include "log.h"

#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <vector>

#include "acl/acl_rt_compile.h"

RtcKernelLoader::~RtcKernelLoader() {
  Unload();
}

std::string RtcKernelLoader::GetCurrentLibraryDir() {
  Dl_info info{};
  if ((dladdr(reinterpret_cast<void *>(&GetCurrentLibraryDir), &info) == 0) || (info.dli_fname == nullptr)) {
    return {};
  }
  const std::string library_path = info.dli_fname;
  const auto pos = library_path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0) {
    return "/";
  }
  return library_path.substr(0, pos);
}

std::string RtcKernelLoader::LoadTextFromFile(const std::string &file_path) {
  std::ifstream file(file_path, std::ios::in);
  if (!file) {
    return {};
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::string RtcKernelLoader::GetRtcCompileOption() {
  int32_t device_id = 0;
  aclrtGetDevice(&device_id);

  int64_t npu_arch = 0;
  aclError ret = aclrtGetDeviceInfo(device_id, ACL_DEV_ATTR_NPU_ARCH, &npu_arch);
  if (ret != ACL_ERROR_NONE) {
    LOG_WARNING("Failed to get NPU arch, error: ", ret, ", using default dav-2201");
    return "--npu-arch=dav-2201";
  }

  return "--npu-arch=dav-" + std::to_string(npu_arch);
}

ge::graphStatus RtcKernelLoader::CompileKernel(const char *kernel_name, const std::string &source,
                                               std::vector<char> &bin_data) {
  aclrtcProg prog = nullptr;
  aclError ret = aclrtcCreateProg(&prog, source.c_str(), kernel_name, 0, nullptr, nullptr);
  if (ret != ACL_ERROR_NONE) {
    LOG_ERROR("aclrtcCreateProg failed, error: ", ret);
    return ge::GRAPH_FAILED;
  }

  const std::string compile_option = GetRtcCompileOption();
  const char *options[] = {compile_option.c_str()};
  ret = aclrtcCompileProg(prog, 1, options);
  if (ret != ACL_ERROR_NONE) {
    LOG_ERROR("aclrtcCompileProg failed, error: ", ret);
    aclrtcDestroyProg(&prog);
    return ge::GRAPH_FAILED;
  }

  size_t bin_size = 0;
  ret = aclrtcGetBinDataSize(prog, &bin_size);
  if (ret != ACL_ERROR_NONE) {
    LOG_ERROR("aclrtcGetBinDataSize failed, error: ", ret);
    aclrtcDestroyProg(&prog);
    return ge::GRAPH_FAILED;
  }

  bin_data.resize(bin_size);
  ret = aclrtcGetBinData(prog, bin_data.data());
  if (ret != ACL_ERROR_NONE) {
    LOG_ERROR("aclrtcGetBinData failed, error: ", ret);
    aclrtcDestroyProg(&prog);
    return ge::GRAPH_FAILED;
  }
  aclrtcDestroyProg(&prog);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus RtcKernelLoader::LoadBinary(const char *kernel_name, const std::vector<char> &bin_data) {
  aclrtBinaryLoadOption binary_load_option;
  binary_load_option.type = ACL_RT_BINARY_LOAD_OPT_MAGIC;
  binary_load_option.value.magic = ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE;
  aclrtBinaryLoadOptions binary_load_options;
  binary_load_options.numOpt = 1;
  binary_load_options.options = &binary_load_option;

  aclError ret = aclrtBinaryLoadFromData(bin_data.data(), bin_data.size(), &binary_load_options, &bin_handle_);
  if (ret != ACL_ERROR_NONE) {
    LOG_ERROR("aclrtBinaryLoadFromData failed, error: ", ret);
    return ge::GRAPH_FAILED;
  }

  ret = aclrtBinaryGetFunction(bin_handle_, kernel_name, &func_handle_);
  if (ret != ACL_ERROR_NONE) {
    LOG_ERROR("aclrtBinaryGetFunction failed, error: ", ret);
    aclrtBinaryUnLoad(bin_handle_);
    bin_handle_ = nullptr;
    return ge::GRAPH_FAILED;
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus RtcKernelLoader::Load(const char *kernel_name, const char *source_file) {
  if (func_handle_ != nullptr) {
    return ge::GRAPH_SUCCESS;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (func_handle_ != nullptr) {
    return ge::GRAPH_SUCCESS;
  }

  const auto source_path = GetCurrentLibraryDir() + "/" + source_file;
  const auto source = LoadTextFromFile(source_path);
  if (source.empty()) {
    LOG_ERROR("Failed to load kernel source from: ", source_path);
    return ge::GRAPH_FAILED;
  }

  std::vector<char> bin_data;
  if (CompileKernel(kernel_name, source, bin_data) != ge::GRAPH_SUCCESS) {
    return ge::GRAPH_FAILED;
  }

  return LoadBinary(kernel_name, bin_data);
}

void RtcKernelLoader::Unload() {
  if (bin_handle_ != nullptr) {
    aclrtBinaryUnLoad(bin_handle_);
    bin_handle_ = nullptr;
    func_handle_ = nullptr;
  }
}
