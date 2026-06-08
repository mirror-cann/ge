/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "macro_utils/dt_public_scope.h"
#include <kernel/memory/mem_block.h>
#include "kernel/memory/caching_mem_allocator.h"
#include "stub/gert_runtime_stub.h"
#include "aicore/launch_kernel/rt_kernel_launch_args_ex.h"
#include "aicore/launch_kernel/ai_core_launch_kernel.h"
#include "kernel/memory/multi_stream_mem_block.h"
#include "kernel/memory/single_stream_l2_allocator.h"
#include "faker/kernel_run_context_facker.h"
#include "register/kernel_registry.h"
#include "runtime/rt_ffts_plus.h"
#include "graph/op_kernel_bin.h"
#include "graph_builder/bg_tiling.h"
#include "exe_graph/runtime/gert_tensor_data.h"
#include "common/dump/exception_dumper.h"
#include "subscriber/profiler/cann_profiler_v2.h"
#include "runtime/subscriber/global_dumper.h"
#include "depends/profiler/src/dump_stub.h"
#include "macro_utils/dt_public_unscope.h"
#include "common/kernel_handles_manager/kernel_handles_manager.h"
#include "common/kernel_handles_manager/aicore_kernel_handles_manager.h"
namespace gert {
using namespace ge;
using namespace kernel;

namespace kernel {
extern ge::graphStatus AiCoreLaunchKernelV2(KernelContext *context);
extern ge::graphStatus AiCoreLaunchMixKernelV2(KernelContext *context);
}
namespace {
using ComputeNodeDesc = RtKernelLaunchArgsEx::ComputeNodeDesc;
using CustAICPUKernelPtr = std::shared_ptr<ge::OpKernelBin>;
struct FakeRuntimeGetFuncValue : AclRuntimeStubImpl {
  aclError aclrtBinaryGetFunction(const aclrtBinHandle binHandle, const char *kernelName,
                                  aclrtFuncHandle *funcHandle) override {
    *funcHandle = reinterpret_cast<void *>(0x1600);
    return ACL_SUCCESS;
  }
  aclError aclrtBinaryGetFunctionByEntry(aclrtBinHandle binHandle,
                                         uint64_t funcEntry,
                                         aclrtFuncHandle *funcHandle) override {
    *funcHandle = reinterpret_cast<void *>(0x1600);
    return ACL_SUCCESS;
  }
};

void CreateDefaultArgsInfo(ArgsInfosDesc::ArgsInfo *args_info, size_t input_num, size_t output_num) {
  auto node_io_num = input_num + output_num;
  for (size_t idx = 0U; idx < node_io_num; ++idx) {
    int32_t start_index = (idx < input_num) ? idx : (idx - input_num);
    auto arg_type = (idx < input_num) ? ArgsInfosDesc::ArgsInfo::ArgsInfoType::INPUT
                                      : ArgsInfosDesc::ArgsInfo::ArgsInfoType::OUTPUT;
    args_info[idx].Init(arg_type, ArgsInfosDesc::ArgsInfo::ArgsInfoFormat::DIRECT_ADDR, start_index, 1U);
    if (input_num > 1 && idx == 1) {
      args_info[idx].folded_first = true;
      args_info[idx].arg_size = 1;
    }
  }
}
std::unique_ptr<uint8_t[]> CreateDefaultArgsInfoDesc(size_t input_num, size_t output_num) {
  const size_t args_info_num = input_num + output_num;
  ArgsInfosDesc::ArgsInfo args_info[args_info_num];
  CreateDefaultArgsInfo(args_info, input_num, output_num);
  size_t total_size = 0U;
  const size_t args_info_size = args_info_num * sizeof(ArgsInfosDesc::ArgsInfo);
  auto args_info_desc_holder = ArgsInfosDesc::Create(args_info_size, total_size);
  auto args_info_desc = reinterpret_cast<ArgsInfosDesc *>(args_info_desc_holder.get());
  if (args_info_size > 0) {
    GELOGD("Copy args info to compute node extended desc mem, size:%lld", args_info_size);
    GE_ASSERT_EOK(
        memcpy_s(args_info_desc->MutableArgsInfoBase(), args_info_desc->GetArgsInfoSize(), args_info, args_info_size));
  }
  args_info_desc->Init(input_num, output_num, input_num, output_num);
  return args_info_desc_holder;
}

void CreateMixArgsInfo(ArgsInfosDesc::ArgsInfo *args_info, size_t input_num, size_t output_num, size_t idx0_start_index) {
  auto node_io_num = input_num + output_num;
  args_info[0].Init(ArgsInfosDesc::ArgsInfo::ArgsInfoType::INPUT,
    ArgsInfosDesc::ArgsInfo::ArgsInfoFormat::DIRECT_ADDR, idx0_start_index, 1U);
  for (size_t idx = 1U; idx < node_io_num; ++idx) {
    int32_t start_index = (idx < input_num) ? idx : (idx - input_num);
    auto arg_type = (idx < input_num) ? ArgsInfosDesc::ArgsInfo::ArgsInfoType::INPUT
                                      : ArgsInfosDesc::ArgsInfo::ArgsInfoType::OUTPUT;
    args_info[idx].Init(arg_type, ArgsInfosDesc::ArgsInfo::ArgsInfoFormat::DIRECT_ADDR, start_index, 1U);
    if (input_num > 1 && idx == 1) {
      args_info[idx].folded_first = true;
      args_info[idx].arg_size = 1;
    }
  }
}

std::unique_ptr<uint8_t[]> CreateMixArgsInfoDesc(size_t input_num, size_t output_num, size_t idx0_start_index) {
  const size_t args_info_num = input_num + output_num;
  ArgsInfosDesc::ArgsInfo args_info[args_info_num];
  CreateMixArgsInfo(args_info, input_num, output_num, idx0_start_index);
  size_t total_size = 0U;
  const size_t args_info_size = args_info_num * sizeof(ArgsInfosDesc::ArgsInfo);
  auto args_info_desc_holder = ArgsInfosDesc::Create(args_info_size, total_size);
  auto args_info_desc = reinterpret_cast<ArgsInfosDesc *>(args_info_desc_holder.get());
  if (args_info_size > 0) {
    GELOGD("Copy args info to compute node extended desc mem, size:%lld", args_info_size);
    GE_ASSERT_EOK(
        memcpy_s(args_info_desc->MutableArgsInfoBase(), args_info_desc->GetArgsInfoSize(), args_info, args_info_size));
  }
  args_info_desc->Init(input_num, output_num, input_num, output_num);
  return args_info_desc_holder;
}
}
class AiKernelLaunchST : public testing::Test {
 public:
  KernelRegistry &registry = KernelRegistry::GetInstance();
};
constexpr uint32_t MAX_IO_NUM = 10;
const TensorAddress g_overflow_addr = reinterpret_cast<TensorAddress>(0x66);
const TensorAddress g_shape_buffer_addr = reinterpret_cast<TensorAddress>(0x11);
struct WorkerSpace {
  size_t capacity_ = 1;
  size_t size_ = 0;
  uint8_t elements[8];
};

constexpr uint64_t stub_mem_hbm_addr = 0x22;
struct FakeMemBlock {
  FakeMemBlock(size_t size, memory::MultiStreamMemBlock *addr) : capacity_(size), size_(size) {
    for (size_t i = 0; i < size && i < 8U; i++) {
      mem_block[i] = addr;
    }
  }
  size_t capacity_;
  size_t size_;
  memory::MultiStreamMemBlock *mem_block[8];
};

struct AiKernelLaunchContext {
  AiKernelLaunchContext(size_t input_num, size_t output_num, uint64_t io_addr, const Shape &shape,
                        bool is_mix, bool is_append_addr, size_t idx0_start_index)
      : io_num(input_num + output_num),
        output_num_(output_num),
        node_info("node"),
        block(stub_allocator, reinterpret_cast<void *>(stub_mem_hbm_addr), 100),
        tensor_addr(reinterpret_cast<void *>(io_addr)),
        is_mix_vec(is_mix){
    stream = reinterpret_cast<void *>(0x11);
    handle = reinterpret_cast<void *>(0x12);
    schem = 0;
    dev_fun = 100;
    size_t actual_cfg_num = 0UL;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
    attrs[actual_cfg_num].value.schemMode = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
    attrs[actual_cfg_num].value.dynUBufSize = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
    attrs[actual_cfg_num].value.engineType = ACL_RT_ENGINE_TYPE_AIC;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
    attrs[actual_cfg_num].value.blockDimOffset = 1U;
    actual_cfg_num++;
    cfg.attrs = &attrs[0];
    cfg.numAttrs = actual_cfg_num;
    ComputeNodeDesc node_desc = {.input_num = input_num,
                                 .output_num = output_num,
                                 .workspace_cap = 8,
                                 .max_tiling_data = 128,
                                 .need_shape_buffer = false,
                                 .need_overflow = false,
                                 .compiled_args_size = 0};
    auto args_info_desc_holder = CreateDefaultArgsInfoDesc(input_num, output_num);
    if (is_append_addr) {
      args_info_desc_holder = CreateMixArgsInfoDesc(input_num, output_num, idx0_start_index);
    }
    auto args_info_desc = reinterpret_cast<ArgsInfosDesc *>(args_info_desc_holder.get());
    launch_args = RtKernelLaunchArgsEx::Create(node_desc, *args_info_desc);
    auto launch_args_ex = reinterpret_cast<RtKernelLaunchArgsEx *>(launch_args.get());
    auto &io_args_info = launch_args_ex->GetIoArgsInfo();
    auto io_arg_num = io_args_info.GetIoArgNum();
    if (io_arg_num > 2) {
      auto io_arg = io_args_info.GetIoArgByIndex(1);
      gert::IoArgsInfo::IoArg tmp_io_arg = {io_arg->arg_offset, io_arg->start_index, io_arg->arg_num,
          io_arg->is_input, false, io_arg->folded_first, io_arg->dyn_desc, io_arg->data_size};
      io_args_info.SetIoArgByIndex(1, tmp_io_arg);
    }
    work_space = ContinuousVector::Create<GertTensorData *>(0);
    clean_work_space = ContinuousVector::Create<GertTensorData *>(0);
    store_shape.MutableStorageShape() = shape;
    memory::SingleStreamL2Allocator single_stream_l2_allocator{&stub_allocator};
    mem_block.ReInit(&single_stream_l2_allocator, &block, memory::BlockAllocType(memory::BlockAllocType::kNorm, 0U));
    work_space_gtd = GertTensorData(mem_block.GetAddr(), mem_block.GetSize(), TensorPlacement::kOnDeviceHbm, 0);
    InitAicoreRegisterInfo();
  }

  AiKernelLaunchContext(size_t input_num, size_t output_num, uint64_t io_addr, const Shape &shape,
                        bool is_mix = false)
      : io_num(input_num + output_num),
        output_num_(output_num),
        node_info("node"),
        block(stub_allocator, reinterpret_cast<void *>(stub_mem_hbm_addr), 100),
        tensor_addr(reinterpret_cast<void *>(io_addr)),
        is_mix_vec(is_mix){
    stream = reinterpret_cast<void *>(0x11);
    handle = reinterpret_cast<void *>(0x12);
    schem = 0;
    dev_fun = 100;
    size_t actual_cfg_num = 0UL;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
    attrs[actual_cfg_num].value.schemMode = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
    attrs[actual_cfg_num].value.dynUBufSize = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
    attrs[actual_cfg_num].value.engineType = ACL_RT_ENGINE_TYPE_AIC;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
    attrs[actual_cfg_num].value.blockDimOffset = 1U;
    actual_cfg_num++;
    cfg.attrs = &attrs[0];
    cfg.numAttrs = actual_cfg_num;
    dfx_arg = {true, true, 12345};
    ComputeNodeDesc node_desc = {.input_num = input_num,
                                 .output_num = output_num,
                                 .workspace_cap = 8,
                                 .max_tiling_data = 128,
                                 .need_shape_buffer = false,
                                 .need_overflow = false,
                                 .compiled_args_size = 0};
    auto args_info_desc_holder = CreateDefaultArgsInfoDesc(input_num, output_num);
    auto args_info_desc = reinterpret_cast<ArgsInfosDesc *>(args_info_desc_holder.get());
    launch_args = RtKernelLaunchArgsEx::Create(node_desc, *args_info_desc);
    work_space = ContinuousVector::Create<GertTensorData *>(0);
    clean_work_space = ContinuousVector::Create<GertTensorData *>(0);
    store_shape.MutableStorageShape() = shape;
    memory::SingleStreamL2Allocator single_stream_l2_allocator{&stub_allocator};
    mem_block.ReInit(&single_stream_l2_allocator, &block, memory::BlockAllocType(memory::BlockAllocType::kNorm, 0U));
    work_space_gtd = GertTensorData(mem_block.GetAddr(), mem_block.GetSize(), TensorPlacement::kOnDeviceHbm, 0);
    InitAicoreRegisterInfo();
  }
  AiKernelLaunchContext(size_t input_num, size_t output_num, uint64_t io_addr, const Shape &shape, bool need_overflow,
                        TensorAddress overflow_addr, bool is_mix = false)
      : io_num(input_num + output_num),
        output_num_(output_num),
        node_info("node"),
        block(stub_allocator, reinterpret_cast<void *>(stub_mem_hbm_addr), 100),
        tensor_addr(reinterpret_cast<void *>(io_addr)),
        is_mix_vec(is_mix) {
    stream = reinterpret_cast<void *>(0x11);
    handle = reinterpret_cast<void *>(0x12);
    schem = 0;
    dev_fun = 100;
    size_t actual_cfg_num = 0UL;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
    attrs[actual_cfg_num].value.schemMode = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
    attrs[actual_cfg_num].value.dynUBufSize = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
    attrs[actual_cfg_num].value.engineType = ACL_RT_ENGINE_TYPE_AIC;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
    attrs[actual_cfg_num].value.blockDimOffset = 1U;
    actual_cfg_num++;
    cfg.attrs = &attrs[0];
    cfg.numAttrs = actual_cfg_num;
    dfx_arg = {true, true, 12345};
    ComputeNodeDesc node_desc = {.input_num = input_num,
        .output_num = output_num,
        .workspace_cap = 8,
        .max_tiling_data = 128,
        .need_shape_buffer = false,
        .need_overflow = need_overflow,
        .compiled_args_size = 0};
    auto args_info_desc_holder = CreateDefaultArgsInfoDesc(input_num, output_num);
    auto args_info_desc = reinterpret_cast<ArgsInfosDesc *>(args_info_desc_holder.get());
    launch_args = RtKernelLaunchArgsEx::Create(node_desc, *args_info_desc);
    auto launch_args_ex = reinterpret_cast<RtKernelLaunchArgsEx *>(launch_args.get());
    *launch_args_ex->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kOverflowAddr) = overflow_addr;
    work_space = ContinuousVector::Create<GertTensorData *>(0);
    clean_work_space = ContinuousVector::Create<GertTensorData *>(0);
    store_shape.MutableStorageShape() = shape;
    memory::SingleStreamL2Allocator single_stream_l2_allocator{&stub_allocator};
    mem_block.ReInit(&single_stream_l2_allocator, &block, memory::BlockAllocType(memory::BlockAllocType::kNorm, 0U));
    work_space_gtd = GertTensorData(mem_block.GetAddr(), mem_block.GetSize(), TensorPlacement::kOnDeviceHbm, 0);
    InitAicoreRegisterInfo();
  }

  AiKernelLaunchContext(size_t input_num, size_t output_num, uint64_t io_addr, const Shape &shape, bool need_overflow,
                        TensorAddress overflow_addr, bool need_shape_buffer, TensorAddress shape_buffer_addr,
                        bool is_mix = false)
      : io_num(input_num + output_num),
        output_num_(output_num),
        node_info("node"),
        block(stub_allocator, reinterpret_cast<void *>(stub_mem_hbm_addr), 100),
        tensor_addr(reinterpret_cast<void *>(io_addr)),
        is_mix_vec(is_mix) {
    stream = reinterpret_cast<void *>(0x11);
    handle = reinterpret_cast<void *>(0x12);
    schem = 0;
    dev_fun = 100;
    size_t actual_cfg_num = 0UL;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
    attrs[actual_cfg_num].value.schemMode = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
    attrs[actual_cfg_num].value.dynUBufSize = 0U;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
    attrs[actual_cfg_num].value.engineType = ACL_RT_ENGINE_TYPE_AIC;
    actual_cfg_num++;
    attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
    attrs[actual_cfg_num].value.blockDimOffset = 1U;
    actual_cfg_num++;
    cfg.attrs = &attrs[0];
    cfg.numAttrs = actual_cfg_num;
    dfx_arg = {true, true, 12345};
    ComputeNodeDesc node_desc = {.input_num = input_num,
        .output_num = output_num,
        .workspace_cap = 8,
        .max_tiling_data = 128,
        .need_shape_buffer = need_shape_buffer,
        .need_overflow = need_overflow,
        .compiled_args_size = 0};
    auto args_info_desc_holder = CreateDefaultArgsInfoDesc(input_num, output_num);
    auto args_info_desc = reinterpret_cast<ArgsInfosDesc *>(args_info_desc_holder.get());
    launch_args = RtKernelLaunchArgsEx::Create(node_desc, *args_info_desc);
    auto launch_args_ex = reinterpret_cast<RtKernelLaunchArgsEx *>(launch_args.get());
    *launch_args_ex->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kOverflowAddr) = overflow_addr;
    *launch_args_ex->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kShapeBufferAddr) = shape_buffer_addr;
    work_space = ContinuousVector::Create<GertTensorData *>(0);
    clean_work_space = ContinuousVector::Create<GertTensorData *>(0);
    store_shape.MutableStorageShape() = shape;
    memory::SingleStreamL2Allocator single_stream_l2_allocator{&stub_allocator};
    mem_block.ReInit(&single_stream_l2_allocator, &block, memory::BlockAllocType(memory::BlockAllocType::kNorm, 0U));
    work_space_gtd = GertTensorData(mem_block.GetAddr(), mem_block.GetSize(), TensorPlacement::kOnDeviceHbm, 0);
    InitAicoreRegisterInfo();
  }

  void InitAicoreRegisterInfo() {
    const char test_bin[] = "test_kernel_bin";
    std::vector<char> buffer(test_bin, test_bin + strlen(test_bin));
    kernel_bin_ptr = std::make_shared<ge::OpKernelBin>("test_kernel", std::move(buffer));
    aicore_register_info.kernel_bin_name = "test_kernel";
    aicore_register_info.magic = RT_DEV_BINARY_MAGIC_ELF_AIVEC;
    aicore_register_info.kernel_bin = kernel_bin_ptr;
  }

  KernelContext *WithFlag(uint64_t block_dim = 1) {
    constexpr auto fixed_input_num = static_cast<size_t>(kernel::WithArgs::kIoAddrs);
    size_t in_num = fixed_input_num + io_num + io_num;
    if (is_mix_vec) {
      in_num += 4;
      t_block_dim = block_dim;
    }
    context = BuildKernelRunContext(in_num, 1);
    InitCommonInput(io_num);
    if (io_num > MAX_IO_NUM) {
      return context;
    }
    for (size_t i = 0; i < io_num; i++) {
      tensor_datas[i] = {tensor_addr, 0U, kTensorPlacementEnd, -1};
      context.value_holder[i + fixed_input_num].Set(&tensor_datas[i], nullptr);
      context.value_holder[i + fixed_input_num + io_num].Set(&store_shape, nullptr);
    }
    if (is_mix_vec) {
      mix_args.mix_type = kernel::MixType::MIX_AICORE;
      mix_args.all_core_num = 15;
      mix_args.ai_core_num = 8;
      mix_args.vec_core_num = 7;
      context.value_holder[fixed_input_num + io_num + io_num + 0].Set(&mix_args, nullptr);
      context.value_holder[fixed_input_num + io_num + io_num + 1].Set(sub_stream, nullptr);
      context.value_holder[fixed_input_num + io_num + io_num + 2].Set(rt_notify1, nullptr);
      context.value_holder[fixed_input_num + io_num + io_num + 3].Set(rt_notify2, nullptr);
    }
    return context;
  }

  AiKernelLaunchContext &WorkSpace(size_t size) {
    work_space = ContinuousVector::Create<GertTensorData *>(size);
    auto work_space_vector = reinterpret_cast<ContinuousVector *>(work_space.get());
    work_space_vector->SetSize(size);
    auto work_space_ptr = reinterpret_cast<GertTensorData **>(work_space_vector->MutableData());
    for (size_t i = 0; i < work_space_vector->GetSize(); i++) {
      work_space_ptr[i] = &work_space_gtd;
    }
    return *this;
  }

  AiKernelLaunchContext &CleanWorkSpace(size_t size) {
    clean_work_space = ContinuousVector::Create<GertTensorData *>(size);
    auto work_space_vector = reinterpret_cast<ContinuousVector *>(clean_work_space.get());
    work_space_vector->SetSize(size);
    auto work_space_ptr = reinterpret_cast<GertTensorData **>(work_space_vector->MutableData());
    for (size_t i = 0; i < work_space_vector->GetSize(); i++) {
      work_space_ptr[i] = &work_space_gtd;
    }
    return *this;
  }

  AiKernelLaunchContext &CleanWorkspaceIndex(const std::vector<int64_t> &workspace_index) {
    clean_ws_index = ContinuousVector::Create<int64_t>(workspace_index.size());
    auto vec = reinterpret_cast<ContinuousVector *>(clean_ws_index.get());
    vec->SetSize(workspace_index.size());
    for (size_t i = 0U; i < workspace_index.size(); ++i) {
      *(reinterpret_cast<int64_t *>(vec->MutableData()) + i) = workspace_index[i];
    }
    return *this;
  }

  KernelContext *WithAtomicFlag() {
    constexpr auto fixed_input_num = static_cast<size_t>(kernel::WithAtomic::kIoAddrs);
    context = BuildKernelRunContext(fixed_input_num + output_num_ + 1, 1);
    InitCommonInput(output_num_);
    context.value_holder[static_cast<size_t>(kernel::WithAtomic::kWorkspaceIndex)].Set(
        reinterpret_cast<void *>(clean_ws_index.get()), nullptr);
    if (output_num_ > MAX_IO_NUM) {
      return context;
    }
    for (size_t i = 0; i < output_num_; i++) {
      tensor_datas[i] = {tensor_addr, 0U, kTensorPlacementEnd, -1};
      context.value_holder[fixed_input_num + i].Set(reinterpret_cast<void *>(&tensor_datas[i]), nullptr);
    }
    context.value_holder[fixed_input_num + output_num_].Set(clean_work_space.get(), nullptr);
    return context;
  }

  void InitCommonInput(uint64_t add_num) {
    context.value_holder[0].Set(stream, nullptr);
    context.value_holder[1].Set(reinterpret_cast<void *>(&aicore_register_info), nullptr);
    context.value_holder[2].Set(reinterpret_cast<void *>(t_block_dim), nullptr);
    context.value_holder[3].Set(work_space.get(), nullptr);
    context.value_holder[4].Set(nullptr, nullptr);
    context.value_holder[5].Set(reinterpret_cast<void *>(&attrs[0]), nullptr);
    context.value_holder[6].Set(reinterpret_cast<void *>(&cfg), nullptr);
    context.value_holder[7].Set(reinterpret_cast<void *>(add_num), nullptr);
    context.value_holder[8].Set(reinterpret_cast<void *>(schem), nullptr);
    context.value_holder[9].Set(reinterpret_cast<void *>(&dfx_arg), nullptr);
    context.value_holder[10].Set(reinterpret_cast<void *>(launch_args.get()), nullptr);
    context.value_holder[11].Set(reinterpret_cast<void *>(local_mem_size), nullptr);
    context.value_holder[12].Set(reinterpret_cast<void *>(dev_fun), nullptr);
    context.value_holder[13].Set(reinterpret_cast<void *>(const_cast<char*>(kernel_name.c_str())), nullptr);
    context.value_holder[14].Set(reinterpret_cast<void *>(0U), nullptr);
    context.value_holder[15].Set(reinterpret_cast<void *>(&aicore_kernel_handles_manager), nullptr);
  }
  gert::memory::CachingMemAllocator stub_allocator{0, 1};

  KernelRunContextHolder context;
  void *stream;
  ge::AicoreRegisterInfo aicore_register_info;
  ge::OpKernelBinPtr kernel_bin_ptr;
  void *handle;
  std::string kernel_name = "kernel_name";
  uint64_t t_block_dim;
  uint64_t schem;
  uint64_t dev_fun;
  gert::DfxExeArg dfx_arg;
  uint32_t local_mem_size = 200;
  uint64_t io_num;
  uint64_t output_num_;
  aclrtLaunchKernelAttr attrs[4];
  aclrtLaunchKernelCfg cfg;
  AicoreKernelHandlesManager aicore_kernel_handles_manager;
  std::string node_info;
  GertTensorData tensor_datas[10];
  WorkerSpace worker_space;
  memory::MultiStreamMemBlock mem_block;
  ge::MemBlock block;
  GertTensorData work_space_gtd;
  void *tensor_addr;
  bool is_mix_vec;
  uint32_t all_core_num = 15;
  uint32_t ai_core_num = 8;
  uint32_t v_core_num = 7;
  StorageShape store_shape;
  std::unique_ptr<uint8_t[]> work_space;
  std::unique_ptr<uint8_t[]> clean_work_space;
  std::unique_ptr<uint8_t[]> launch_args;
  std::unique_ptr<uint8_t[]> clean_ws_index;
  kernel::MixCoreArgs mix_args;
  rtStream_t sub_stream = reinterpret_cast<void *>(0x11);
  rtNotify_t rt_notify1 = reinterpret_cast<void *>(0x11);
  rtNotify_t rt_notify2 = reinterpret_cast<void *>(0x11);
};

TEST_F(AiKernelLaunchST, test_ai_AiCoreLaunchKernelV2_run_success) {
  dlog_setlevel(GE_MODULE_NAME, DLOG_DEBUG, 0);
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}));
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_NE(registry.FindKernelFuncs("LaunchKernelV2"), nullptr);
  ASSERT_EQ(kernel::AiCoreLaunchKernelV2(context.WithFlag()), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);
  auto ret = registry.FindKernelFuncs("LaunchKernelV2")->trace_printer(context.WithFlag());
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchMixKernelV2_run_success) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}), true);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_NE(registry.FindKernelFuncs("LaunchMixKernelV2"), nullptr);
  ASSERT_EQ(kernel::AiCoreLaunchMixKernelV2(context.WithFlag(15)), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);

  auto ret = registry.FindKernelFuncs("LaunchMixKernelV2")->trace_printer(context.WithFlag());
  // profiling
  ProfNodeAdditionInfo info;
  CannProfilingInfoWrapper prof_info(&info);
  registry.FindKernelFuncs("LaunchMixKernelV2")->profiling_info_filler(context.WithFlag(), prof_info);
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchMixKernelV2_run_success1) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}), true);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_NE(registry.FindKernelFuncs("LaunchMixKernelV2"), nullptr);
  ASSERT_EQ(kernel::AiCoreLaunchMixKernelV2(context.WithFlag(8)), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);
}

TEST_F(AiKernelLaunchST, AiCoreLaunchKernelV2_need_overflow_success) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}), true, g_overflow_addr);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_EQ(kernel::AiCoreLaunchKernelV2(context.WithFlag()), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  auto args_host_buffer = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  for (size_t i = 0U; i < context.io_num; ++i) {
    ASSERT_EQ(args_host_buffer[i], 0x11);
  }
}

TEST_F(AiKernelLaunchST, AiCoreLaunchKernelV2_need_overflow_and_workspaces_success) {
  AiKernelLaunchContext context(4U, 2U, 0x11, Shape({2, 2, 3}),true, g_overflow_addr);
  context.WorkSpace(2U);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_EQ(kernel::AiCoreLaunchKernelV2(context.WithFlag()), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  auto args_host_buffer = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  for (size_t i = 0U; i < context.io_num; ++i) {
    ASSERT_EQ(args_host_buffer[i], 0x11);
  }
  auto workspace_offset = context.io_num;
  ASSERT_NE(reinterpret_cast<uint64_t *>(args_host_buffer[workspace_offset]), nullptr);
  EXPECT_EQ(args_host_buffer[workspace_offset], 0x22);
  EXPECT_EQ(args_host_buffer[workspace_offset + 1U], 0x22);
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchKernelV2_input_check_faild) {
  auto run_context = BuildKernelRunContext(6, 1);
  ASSERT_NE(registry.FindKernelFuncs("LaunchKernelV2"), nullptr);
  ASSERT_NE(registry.FindKernelFuncs("LaunchKernelV2")->run_func(run_context), ge::GRAPH_SUCCESS);
}

TEST_F(AiKernelLaunchST, test_ai_AiCoreLaunchKernelV2_run_failed) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}));
  struct FakeRuntime : AclRuntimeStubImpl {
    aclError aclrtLaunchKernelWithHostArgs(aclrtFuncHandle funcHandle, uint32_t numBlocks,
            aclrtStream stream, aclrtLaunchKernelCfg *cfg, void *hostArgs, size_t argsSize,
            aclrtPlaceHolderInfo *placeHolderArray, size_t placeHolderNum) {
      return 0x01;
    }
  };
  GertRuntimeStub stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntime()));
  ASSERT_EQ(registry.FindKernelFuncs("LaunchKernelV2")->run_func(context.WithFlag()), 0x01);
}

TEST_F(AiKernelLaunchST, test_LaunchKernelV2_para_check_failed) {
  auto run_context = BuildKernelRunContext(6, 1);
  ASSERT_NE(registry.FindKernelFuncs("LaunchKernelV2")->run_func(run_context), ge::GRAPH_SUCCESS);
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchKernelV2_run_failed) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}));
  struct FakeRuntime : AclRuntimeStubImpl {
    aclError aclrtLaunchKernelWithHostArgs(aclrtFuncHandle funcHandle, uint32_t numBlocks,
            aclrtStream stream, aclrtLaunchKernelCfg *cfg, void *hostArgs, size_t argsSize,
            aclrtPlaceHolderInfo *placeHolderArray, size_t placeHolderNum) {
      return 0x01;
    }
  };
  GertRuntimeStub stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntime()));
  ASSERT_EQ(registry.FindKernelFuncs("LaunchKernelV2")->run_func(context.WithFlag()), 0x01);
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchKernelV2_run_success) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}));
  context.WorkSpace(2);
  GertRuntimeStub runtime_stub;
  ASSERT_EQ(registry.FindKernelFuncs("LaunchKernelV2")->run_func(context.WithFlag()), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);

  auto ret = registry.FindKernelFuncs("LaunchKernelV2")->trace_printer(context.WithFlag());
  EXPECT_FALSE(ret.empty());
}

TEST_F(AiKernelLaunchST, test_AtomicLaunchKernelV2_para_check_failed) {
  auto run_context = BuildKernelRunContext(6, 1);
  ASSERT_NE(registry.FindKernelFuncs("AtomicLaunchKernelV2")->run_func(run_context), ge::GRAPH_SUCCESS);
}

TEST_F(AiKernelLaunchST, test_AtomicLaunchKernelV2_para_check_run_faild) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}));
  struct FakeRuntime : AclRuntimeStubImpl {
    aclError aclrtLaunchKernelWithHostArgs(aclrtFuncHandle funcHandle, uint32_t numBlocks,
            aclrtStream stream, aclrtLaunchKernelCfg *cfg, void *hostArgs, size_t argsSize,
            aclrtPlaceHolderInfo *placeHolderArray, size_t placeHolderNum) {
      return ACL_SUCCESS;
    }
  };
  GertRuntimeStub stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntime()));
  ASSERT_NE(registry.FindKernelFuncs("AtomicLaunchKernelV2")->run_func(context.WithAtomicFlag()),
            ge::GRAPH_SUCCESS);
}

TEST_F(AiKernelLaunchST, test_AtomicLaunchKernelV2_run_success) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}));
  context.CleanWorkSpace(2);
  GertRuntimeStub runtime_stub;
  ASSERT_EQ(registry.FindKernelFuncs("AtomicLaunchKernelV2")
                ->run_func(context.CleanWorkspaceIndex({}).WithAtomicFlag()),
            ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  ASSERT_EQ(launch_args->GetArgSize(), 88);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);
  ASSERT_NE(io_addrs[1], stub_mem_hbm_addr);
}

TEST_F(AiKernelLaunchST, AtomicLaunchKernelV2_need_overflow_and_workspaces_success) {
  AiKernelLaunchContext context(3, 2, 0x11, Shape({2, 2, 3}), true, g_overflow_addr);
  context.CleanWorkSpace(2);
  GertRuntimeStub runtime_stub;
  ASSERT_EQ(registry.FindKernelFuncs("AtomicLaunchKernelV2")
                ->run_func(context.CleanWorkspaceIndex({0}).WithAtomicFlag()),
            ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  auto args_host_buffer = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  for (size_t i = 0U; i < context.output_num_; ++i) {
    EXPECT_EQ(args_host_buffer[i], 0x11);
  }
  auto workspace_offset = context.output_num_;
  ASSERT_NE(reinterpret_cast<uint64_t *>(args_host_buffer[workspace_offset]), nullptr);
  EXPECT_EQ(args_host_buffer[workspace_offset], 0x22);
}

TEST_F(AiKernelLaunchST, test_AtomicLaunchKernelV2_with_clean_workspace_run_success) {
  AiKernelLaunchContext context(2, 1, 0x11, Shape({2, 2, 3}));
  context.CleanWorkSpace(2);
  GertRuntimeStub runtime_stub;
  ASSERT_EQ(registry.FindKernelFuncs("AtomicLaunchKernelV2")
                ->run_func(context.CleanWorkspaceIndex({0}).WithAtomicFlag()),
            ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  ASSERT_EQ(launch_args->GetArgSize(), 96);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);
  ASSERT_EQ(io_addrs[1], stub_mem_hbm_addr);

  auto ret = registry.FindKernelFuncs("AtomicLaunchKernelV2")
                 ->trace_printer(context.CleanWorkspaceIndex({0}).WithAtomicFlag());
  EXPECT_FALSE(ret.empty());
}

TEST_F(AiKernelLaunchST, test_AtomicLaunchKernelV2_with_profiling_filler) {
  AiKernelLaunchContext context(2, 1, 0x11, Shape({2, 2, 3}));
  context.CleanWorkSpace(2);
  GertRuntimeStub runtime_stub;
  ASSERT_EQ(registry.FindKernelFuncs("AtomicLaunchKernelV2")
                ->run_func(context.CleanWorkspaceIndex({0}).WithAtomicFlag()),
            ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  ASSERT_EQ(launch_args->GetArgSize(), 96);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);
  ASSERT_EQ(io_addrs[1], stub_mem_hbm_addr);

  ProfNodeAdditionInfo info{};
  CannProfilingInfoWrapper wrapper(&info);
  ASSERT_EQ(registry.FindKernelFuncs("AtomicLaunchKernelV2")
                ->profiling_info_filler(context.WithAtomicFlag(), wrapper), ge::SUCCESS);
}

TEST_F(AiKernelLaunchST, test_ffts_task_launch) {
  auto run_context = BuildKernelRunContext(2, 0);
  rtStream_t stream = nullptr;
  rtFftsPlusTaskInfo_t task_info;
  auto *const sqe_ctx = new (std::nothrow) uint8_t[20]{};
  auto *const ffts_plus_sqe = ge::PtrToPtr<uint8_t, rtFftsPlusSqe_t>(sqe_ctx);
  task_info.fftsPlusSqe = ffts_plus_sqe;
  run_context.value_holder[0].Set(stream, nullptr);
  run_context.value_holder[1].Set(&task_info, nullptr);
  ASSERT_EQ(registry.FindKernelFuncs("LaunchFFTSPlusTask")->run_func(run_context), ge::GRAPH_SUCCESS);
  delete [] sqe_ctx;
}

TEST_F(AiKernelLaunchST, AiCoreLaunchKernelV2_Success_EnableLiteExeption) {
  ge::DumpStub::GetInstance().Clear();
  AiKernelLaunchContext context(2, 1, 0x11, Shape({2, 2, 3}));
  context.WorkSpace(2);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_NE(registry.FindKernelFuncs("LaunchKernelV2"), nullptr);
  ASSERT_EQ(kernel::AiCoreLaunchKernelV2(context.WithFlag()), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto io_addrs = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  ASSERT_EQ(io_addrs[0], 0x11);
  EXPECT_EQ(DumpStub::GetInstance().GetUnits().size(), 8);
  EXPECT_EQ(DumpStub::GetInstance().GetUnits()[7][0], 8); // 2 + dy_desc(1) + input size(2) + output size(1) + ws size(2)
  EXPECT_EQ(DumpStub::GetInstance().GetUnits()[7][1], 6); // input size(1) + output size(1)
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchMixKernelV2_run_mix_success) {
  AiKernelLaunchContext context(2, 1, 0x11, Shape({2, 2, 3}), true, true, 0xFFFFU);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_NE(registry.FindKernelFuncs("LaunchMixKernelV2"), nullptr);
  ASSERT_EQ(kernel::AiCoreLaunchMixKernelV2(context.WithFlag(15)), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto ret = registry.FindKernelFuncs("LaunchMixKernelV2")->trace_printer(context.WithFlag());
  // profiling
  ProfNodeAdditionInfo info;
  CannProfilingInfoWrapper prof_info(&info);
  registry.FindKernelFuncs("LaunchMixKernelV2")->profiling_info_filler(context.WithFlag(), prof_info);
}

TEST_F(AiKernelLaunchST, test_AiCoreLaunchMixKernelV2_run_mix_fail) {
  AiKernelLaunchContext context(3, 1, 0x11, Shape({2, 2, 3}), true, true, -1);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_NE(registry.FindKernelFuncs("LaunchMixKernelV2"), nullptr);
  ASSERT_EQ(kernel::AiCoreLaunchMixKernelV2(context.WithFlag(15)), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  ASSERT_EQ(launch_args->GetStream(), context.stream);
  auto ret = registry.FindKernelFuncs("LaunchMixKernelV2")->trace_printer(context.WithFlag());
  // profiling
  ProfNodeAdditionInfo info;
  CannProfilingInfoWrapper prof_info(&info);
  registry.FindKernelFuncs("LaunchMixKernelV2")->profiling_info_filler(context.WithFlag(), prof_info);
}

TEST_F(AiKernelLaunchST, test_LaunchAicpuCustKernel_success) {
  auto run_context = BuildKernelRunContext(2, 1);
  std::string key = "cust_aicpu";
  const char cust_bin[] = "aicpu_cust_bin";
  std::vector<char> buffer(cust_bin, cust_bin + strlen(cust_bin));
  CustAICPUKernelPtr aicpu_kernel_ptr = std::make_shared<ge::OpKernelBin>(key, move(buffer));
  std::string cust_so_name = "libcust_aicpu_kernels.so";
  run_context.value_holder[0].Set(aicpu_kernel_ptr.get(), nullptr);
  run_context.value_holder[1].Set(cust_so_name.data(), nullptr);
  ASSERT_EQ(registry.FindKernelFuncs("LaunchAicpuCustKernel")->run_func(run_context), ge::GRAPH_SUCCESS);
}

TEST_F(AiKernelLaunchST, AiCoreLaunchKernelV2_need_shapebuffer_success) {
  AiKernelLaunchContext context(1, 1, 0x11, Shape({2, 2, 3}), true, g_overflow_addr, true, g_shape_buffer_addr);
  GertRuntimeStub runtime_stub(std::make_unique<RuntimeStubImpl>(), true, std::unique_ptr<AclRuntimeStubImpl>(new FakeRuntimeGetFuncValue()));
  ASSERT_EQ(kernel::AiCoreLaunchKernelV2(context.WithFlag()), ge::GRAPH_SUCCESS);
  auto launch_args = runtime_stub.GetAclRuntimeStub().PopCpuLaunchArgsByKernelName("cpu_new_args_launch_with_place_holder");
  ASSERT_NE(launch_args, nullptr);
  auto args_host_buffer = reinterpret_cast<uint64_t *>(launch_args->GetArgBase());
  for (size_t i = 0U; i < context.io_num; ++i) {
    ASSERT_EQ(args_host_buffer[i], 0x11);
  }
}
}  // namespace gert