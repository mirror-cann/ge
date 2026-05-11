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
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cmath>

#include "add_custom.h"
#include "acl/acl_rt.h"
// 使用aclrtc接口需要包含的头文件
#include "acl/acl_rt_compile.h"
#include "graph/custom_op.h"

using namespace ge;
using KernelBinary = std::vector<uint8_t>;
using KernelBinaryMap = std::map<std::string, KernelBinary>;

namespace kernel_binary_map_utils {
graphStatus Serialize(const KernelBinaryMap &kernel_binary_map, std::vector<uint8_t> &buffer);
graphStatus Deserialize(const std::vector<uint8_t> &buffer, KernelBinaryMap &kernel_binary_map);
}  // namespace kernel_binary_map_utils

namespace compile_utils {
graphStatus BuildRtcCompileOptionFromContext(gert::OpCompileContext *ctx, std::string &rtc_compile_option);
void DumpCompileInputTensor(gert::OpCompileContext *ctx, size_t index);
std::string BuildBinaryKey(const gert::Shape &shape);
void DumpComputeNodeInfo(gert::OpCompileContext *ctx);
void DumpCompileOption(gert::OpCompileContext *ctx, const char *option_key);
void DumpPlatformInfos(gert::OpCompileContext *ctx);
std::string GetKernelSourcePath();
std::string LoadTextFromFile(const std::string &file_path);
}  // namespace compile_utils

#define CHECK_ACL(x)                                                                  \
  do {                                                                                \
    aclError __ret = x;                                                               \
    if (__ret != ACL_ERROR_NONE) {                                                    \
      std::cerr << __FILE__ << ":" << __LINE__ << " aclError:" << __ret << std::endl; \
      return GRAPH_FAILED;                                                            \
    }                                                                                 \
  } while (0)

namespace {
constexpr const char *kCompileOptionDeviceId = "ge.exec.deviceId";
constexpr const char *kCompileOptionJitCompile = "ge.jit_compile";
}  // namespace

class AddCustom : public EagerExecuteOp, public CompilableOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    std::cout << __FILE__ << ":" << __LINE__ << " Execute started, kernel binary count: " << device_elves_.size()
              << std::endl;
    if (device_elves_.empty()) {
      std::cerr << __FILE__ << ":" << __LINE__ << " device_elves_ is empty " << std::endl;
      return GRAPH_FAILED;
    }
    // 获取算子输入 Tensor
    const gert::Tensor *input_x = ctx->GetInputTensor(0);
    const gert::Tensor *input_y = ctx->GetInputTensor(1);
    if ((input_x == nullptr) || (input_y == nullptr)) {
      std::cerr << __FILE__ << ":" << __LINE__ << " input tensor is null, input_x: " << input_x
                << ", input_y: " << input_y << std::endl;
      return GRAPH_FAILED;
    }
    std::cout << __FILE__ << ":" << __LINE__ << " input tensors obtained, input_x addr: " << input_x->GetAddr()
              << ", input_y addr: " << input_y->GetAddr() << std::endl;

    // 申请输出内存
    const gert::StorageShape &output_shape = input_x->GetShape();

    DataType data_type = input_x->GetDataType();
    const gert::StorageFormat &format = input_x->GetFormat();
    gert::Tensor *output_z = ctx->MallocOutputTensor(0, output_shape, format, data_type);
    // 这里按输入 shape 生成 key 选择已下沉的 binary，用来展示多 shape / 多 kernel 的处理框架；
    const auto key = compile_utils::BuildBinaryKey(input_x->GetShape().GetStorageShape());
    if (key.empty()) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to build runtime binary key" << std::endl;
      return GRAPH_FAILED;
    }
    const auto binary_it = device_elves_.find(key);
    if (binary_it == device_elves_.end()) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to find kernel binary for key: " << key << std::endl;
      return GRAPH_FAILED;
    }
    const auto &device_elf = binary_it->second;
    std::cout << __FILE__ << ":" << __LINE__ << " selected kernel binary key: " << key
              << ", bin size: " << device_elf.size() << std::endl;
    // 从二进制数据中获取 Kernel 句柄
    aclrtBinHandle bin_handle = nullptr;
    aclrtFuncHandle func_handle = nullptr;
    aclrtBinaryLoadOption binary_load_option;
    aclrtBinaryLoadOptions binary_load_options;

    binary_load_option.type = ACL_RT_BINARY_LOAD_OPT_MAGIC;
    binary_load_option.value.magic = ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE;

    binary_load_options.numOpt = 1;
    binary_load_options.options = &binary_load_option;

    CHECK_ACL(aclrtBinaryLoadFromData(device_elf.data(), device_elf.size(), &binary_load_options, &bin_handle));
    std::cout << __FILE__ << ":" << __LINE__ << " binary loaded successfully, size: " << device_elf.size() << std::endl;

    CHECK_ACL(aclrtBinaryGetFunction(bin_handle, "add_custom", &func_handle));
    std::cout << __FILE__ << ":" << __LINE__ << " function add_custom obtained successfully" << std::endl;

    // 拼装 args
    struct __attribute__((packed)) {
      const void *arg0 __attribute__((aligned(8)));
      const void *arg1 __attribute__((aligned(8)));
      void *arg2 __attribute__((aligned(8)));
    } args = {input_x->GetAddr(), input_y->GetAddr(), output_z->GetAddr()};

    // 获取需处理的元素个数和 grid
    const int64_t n_elements = input_x->GetShapeSize();
    // 核函数实现中指定的一次性处理的数据块大小
    const int32_t BLOCK_SIZE_VALUE = 1024;
    // 向量加法按照1维网格的方式分块处理，因此需要计算grid_x的值，grid_y/grid_z默认为1
    const int32_t grid_x = std::ceil(static_cast<double>(n_elements) / (BLOCK_SIZE_VALUE));
    const int32_t grid_y = 1;
    const int32_t grid_z = 1;
    const int32_t block_num = grid_x * grid_y * grid_z;

    // 获取 stream
    void *stream = ctx->GetStream();

    CHECK_ACL(aclrtLaunchKernelWithHostArgs(func_handle, static_cast<uint32_t>(block_num), stream, nullptr,
                                            static_cast<void *>(&args), sizeof(args), nullptr, 0));
    std::cout << __FILE__ << ":" << __LINE__ << " kernel launched successfully with block_num: " << block_num
              << std::endl;
    CHECK_ACL(aclrtBinaryUnLoad(bin_handle));
    std::cout << __FILE__ << ":" << __LINE__ << " binary unloaded successfully" << std::endl;
    return GRAPH_SUCCESS;
  }
  graphStatus Compile(gert::OpCompileContext *ctx) override {
    if (ctx == nullptr) {
      std::cerr << __FILE__ << ":" << __LINE__ << " Compile context is null" << std::endl;
      return GRAPH_FAILED;
    }
    compile_utils::DumpCompileOption(ctx, kCompileOptionDeviceId);
    compile_utils::DumpCompileOption(ctx, kCompileOptionJitCompile);
    compile_utils::DumpComputeNodeInfo(ctx);
    compile_utils::DumpPlatformInfos(ctx);
    const auto *const input_x = ctx->GetInputTensor(0U);
    if (input_x == nullptr) {
      std::cerr << __FILE__ << ":" << __LINE__ << " input_x tensor is null when building compile key" << std::endl;
      return GRAPH_FAILED;
    }
    compile_utils::DumpCompileInputTensor(ctx, 0U);
    // 当前 sample 用第一输入 shape 生成 binary key，并按 key 缓存编译产物。
    // 这里已经可以直接拿到输入 tensor 的 shape、data type、format 等元信息。
    // 当前 sample 只提供多 shape / 多 kernel 的处理框架，因此这里仅用 shape 参与 key 生成；
    const auto key = compile_utils::BuildBinaryKey(input_x->GetShape().GetStorageShape());
    if (key.empty()) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to build compile binary key" << std::endl;
      return GRAPH_FAILED;
    }
    const auto source_path = compile_utils::GetKernelSourcePath();
    const auto source = compile_utils::LoadTextFromFile(source_path);
    if (source.empty()) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to load kernel source from: " << source_path << std::endl;
      return GRAPH_FAILED;
    }
    std::cout << __FILE__ << ":" << __LINE__ << " Compile started" << std::endl;
    std::string rtc_compile_option;
    const auto build_option_ret = compile_utils::BuildRtcCompileOptionFromContext(ctx, rtc_compile_option);
    if (build_option_ret != GRAPH_SUCCESS) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to build rtc compile option, ret: " << build_option_ret
                << std::endl;
      return build_option_ret;
    }
    aclrtcProg prog = nullptr;
    CHECK_ACL(aclrtcCreateProg(&prog, source.c_str(), "add_custom", 0, nullptr, nullptr));
    std::cout << __FILE__ << ":" << __LINE__ << " aclrtcCreateProg succeeded" << std::endl;

    // aclrtc流程，结合 OpCompileContext 中的平台信息组装毕昇编译器选项，再调用aclrtcCompileProg进行编译
    const char *options[] = {
        rtc_compile_option.c_str(),
    };
    const int numOptions = sizeof(options) / sizeof(options[0]);
    CHECK_ACL(aclrtcCompileProg(prog, numOptions, options));
    std::cout << __FILE__ << ":" << __LINE__ << " aclrtcCompileProg succeeded" << std::endl;

    // aclrtc流程，获取Device侧二进制内容和大小
    size_t binDataSizeRet;
    CHECK_ACL(aclrtcGetBinDataSize(prog, &binDataSizeRet));
    std::cout << __FILE__ << ":" << __LINE__ << " binary data size: " << binDataSizeRet << std::endl;
    KernelBinary device_elf(binDataSizeRet);
    CHECK_ACL(aclrtcGetBinData(prog, reinterpret_cast<char *>(device_elf.data())));
    const auto existing_binary = device_elves_.find(key);
    if (existing_binary == device_elves_.end()) {
      device_elves_.emplace(key, std::move(device_elf));
      std::cout << __FILE__ << ":" << __LINE__ << " stored new kernel binary, key: " << key
                << ", kernel binary count: " << device_elves_.size() << std::endl;
    } else if (existing_binary->second == device_elf) {
      std::cout << __FILE__ << ":" << __LINE__ << " duplicated compile result for key: " << key
                << ", keep existing kernel binary" << std::endl;
    } else {
      std::cerr << __FILE__ << ":" << __LINE__ << " compile binary key collision, key: " << key
                << ". Current sample only uses the first input shape as key to demonstrate multi-binary management."
                << std::endl;
      aclrtcDestroyProg(&prog);
      return GRAPH_FAILED;
    }
    aclrtcDestroyProg(&prog);
    std::cout << __FILE__ << ":" << __LINE__ << " Compile completed successfully" << std::endl;
    return GRAPH_SUCCESS;
  };

  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    std::cout << __FILE__ << ":" << __LINE__ << " Serialize started, kernel binary count: " << device_elves_.size()
              << std::endl;
    const auto ret = kernel_binary_map_utils::Serialize(device_elves_, buffer);
    if (ret != GRAPH_SUCCESS) {
      return ret;
    }
    std::cout << __FILE__ << ":" << __LINE__ << " Serialize completed, buffer size: " << buffer.size() << std::endl;
    return ret;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    std::cout << __FILE__ << ":" << __LINE__ << " Deserialize started, buffer size: " << buffer.size() << std::endl;
    const auto ret = kernel_binary_map_utils::Deserialize(buffer, device_elves_);
    if (ret != GRAPH_SUCCESS) {
      return ret;
    }
    std::cout << __FILE__ << ":" << __LINE__ << " Deserialize completed, kernel binary count: " << device_elves_.size()
              << std::endl;
    return ret;
  }

 private:
  // 按 shape-derived key 保存多份 binary，供序列化下沉和执行期查找使用
  // 当前 sample 主要提供多 shape / 多 kernel 的处理框架
  KernelBinaryMap device_elves_;
};

REG_AUTO_MAPPING_OP(AddCustom);
