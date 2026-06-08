/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ai_core_launch_kernel.h"
#include <cstddef>
#include <iomanip>
#include "runtime/kernel.h"
#include "runtime/mem.h"
#include "runtime/rt.h"
#include "adump_pub.h"
#include "adump_api.h"
#include "common/kernel_handles_manager/aicore_kernel_handles_manager.h"
#include "graph/ge_error_codes.h"
#include "exe_graph/runtime/tiling_data.h"
#include "graph/def_types.h"
#include "register/kernel_registry.h"
#include "framework/common/debug/log.h"
#include "exe_graph/runtime/tensor.h"
#include "kernel/memory/mem_block.h"
#include "kernel/memory/multi_stream_mem_block.h"
#include "rt_kernel_launch_args_ex.h"
#include "core/debug/kernel_tracing.h"
#include "common/dump/kernel_tracing_utils.h"
#include "common/checker.h"
#include "runtime/context.h"
#include "exe_graph/runtime/gert_tensor_data.h"
#include "exe_graph/runtime/dfx_info_filler.h"
#include "engine/aicore/fe_rt2_common.h"
#include "engine/node_converter_utils.h"
#include "common/dump/exception_dumper.h"
#include "framework/runtime/subscriber/global_dumper.h"
#include "graph/small_vector.h"
#include "aprof_pub.h"
#include "acl/acl_rt.h"

using namespace ge;

namespace gert {
namespace kernel {
namespace {
constexpr uint32_t k2BitsMask = 0x00000003U;
enum class MixKernel {
  kMixArgs,
  kNotifyIds
};

struct LaunchCommonInputs {
  void *stream{nullptr};
  aclrtBinHandle bin_handle{nullptr};
  size_t tiling_key{0};
  char *kernel_name{nullptr};
  const uint32_t *block_dim{nullptr};
  RtKernelLaunchArgsEx *args{nullptr};
  aclrtLaunchKernelAttr *cfg_attrs{nullptr};
  aclrtLaunchKernelCfg *cfg{nullptr};
  uint32_t schedule_mode{0};
  uint32_t local_mem_size{0};
  uint32_t with_handle_flag{0};
};
static_assert(std::is_standard_layout<RtKernelLaunchArgsEx>::value, "The class RtKernelLaunchArgsEx must be a POD");

ge::graphStatus UpdatePlaceHolderInfo(std::vector<aclrtPlaceHolderInfo> &infos, RtKernelLaunchArgsEx *args, bool has_tiling) {
  for (size_t idx = 0U; idx < args->GetBase()->hostInputInfoNum; ++idx) {
    infos[idx].addrOffset = args->GetBase()->hostInputInfoPtr[idx].addrOffset;
    infos[idx].dataOffset = args->GetBase()->hostInputInfoPtr[idx].dataOffset;
  }
  if (has_tiling) {
    infos[args->GetBase()->hostInputInfoNum].addrOffset = args->GetBase()->tilingAddrOffset;
    infos[args->GetBase()->hostInputInfoNum].dataOffset = args->GetTilingDataOffset();
  }
  return ge::GRAPH_SUCCESS;
}

LaunchCommonInputs GetLaunchCommonInputs(KernelContext *context) {
  LaunchCommonInputs inputs;
  inputs.stream = context->GetInputValue<void *>(static_cast<int32_t>(InputCommon::kStream));
  FE_ASSERT_NOTNULL(inputs.stream);
  inputs.bin_handle = context->GetInputValue<aclrtBinHandle>(static_cast<int32_t>(InputCommon::kBinHandle));
  FE_ASSERT_NOTNULL(inputs.bin_handle);
  inputs.tiling_key = context->GetInputValue<size_t>(static_cast<int32_t>(InputCommon::kTilingKey));
  inputs.kernel_name = context->GetInputValue<char *>(static_cast<int32_t>(InputCommon::kKernelName));
  FE_ASSERT_NOTNULL(inputs.kernel_name);
  inputs.block_dim = context->GetInputPointer<uint32_t>(static_cast<int32_t>(InputCommon::kBlockDim));
  FE_ASSERT_NOTNULL(inputs.block_dim);
  inputs.args = context->MutableInputPointer<RtKernelLaunchArgsEx>(static_cast<size_t>(InputCommon::kRtArg));
  FE_ASSERT_NOTNULL(inputs.args);
  inputs.cfg_attrs = context->MutableInputPointer<aclrtLaunchKernelAttr>(static_cast<size_t>(InputCommon::kCfgAttrs));
  FE_ASSERT_NOTNULL(inputs.cfg_attrs);
  inputs.cfg = context->MutableInputPointer<aclrtLaunchKernelCfg>(static_cast<size_t>(InputCommon::kCfg));
  FE_ASSERT_NOTNULL(inputs.cfg);
  inputs.schedule_mode = context->GetInputValue<uint32_t>(static_cast<uint32_t>(InputCommon::kScheduleMode));
  inputs.local_mem_size = context->GetInputValue<uint32_t>(static_cast<uint32_t>(InputCommon::kLocalMemSize));
  inputs.with_handle_flag = context->GetInputValue<uint32_t>(static_cast<uint32_t>(InputCommon::kWithHandleFlag));
  return inputs;
}

ge::graphStatus PrepareLaunchConfig(LaunchCommonInputs &inputs) {
  if (inputs.cfg_attrs == nullptr) {
    GELOGE(ge::FAILED, "cfg_attrs is nullptr");
    return ge::GRAPH_FAILED;
  }
  if (inputs.cfg == nullptr) {
    GELOGE(ge::FAILED, "cfg is nullptr");
    return ge::GRAPH_FAILED;
  }
  inputs.cfg_attrs[0].value.schemMode = static_cast<uint8_t>(inputs.schedule_mode & k2BitsMask);
  inputs.cfg_attrs[1].value.dynUBufSize = inputs.local_mem_size;
  inputs.cfg->attrs = inputs.cfg_attrs;
  return ge::GRAPH_SUCCESS;
}

struct PlaceHolderInfo {
  bool has_tiling{false};
  uint32_t place_holder_num{0};
};

PlaceHolderInfo GetPlaceHolderInfo(RtKernelLaunchArgsEx *args) {
  PlaceHolderInfo info;
  if (args->GetTilingDataOffset() < args->GetBase()->argsSize) {
    info.has_tiling = true;
    info.place_holder_num++;
  }
  info.place_holder_num += args->GetBase()->hostInputInfoNum;
  return info;
}

ge::Status LaunchKernelWithPlaceHolder(aclrtFuncHandle funcHandle, uint32_t block_dim, rtStream_t stream,
                                       aclrtLaunchKernelCfg *cfg, void *hostArgs, uint32_t argsSize,
                                       RtKernelLaunchArgsEx *args, const PlaceHolderInfo &info) {
  if (info.place_holder_num == 0U) {
    aclrtPlaceHolderInfo without_info = {};
    return aclrtLaunchKernelWithHostArgs(funcHandle, block_dim, stream, cfg, hostArgs, argsSize, &without_info, 0U);
  }
  std::vector<aclrtPlaceHolderInfo> infos(info.place_holder_num);
  (void)UpdatePlaceHolderInfo(infos, args, info.has_tiling);
  return aclrtLaunchKernelWithHostArgs(funcHandle, block_dim, stream, cfg, hostArgs, argsSize, infos.data(), info.place_holder_num);
}
const char *kLaunchArgsTypeName[static_cast<uint32_t>(RtKernelLaunchArgsEx::ArgsType::kArgsTypeEnd)] = {
    "ArgsCompiledArgs", "ArgsInputsAddr", "ArgsOutputsAddr", "ShapeBufferAddr",
    "WorkspacesAddr", "TilingDataAddr", "OverflowAddr", "TilingData",
};
static_assert(sizeof(kLaunchArgsTypeName) / sizeof(kLaunchArgsTypeName[0]) ==
              static_cast<uint32_t>(RtKernelLaunchArgsEx::ArgsType::kArgsTypeEnd),
              "The count of names in kLaunchArgsTypeName must match with ArgsType");

struct MixVectorProf {
  uint32_t main_blk_dim{0};
  uint32_t sub_blk_dim{0};
  uint64_t main_begin_time{0};
  uint64_t main_end_time{0};
  uint64_t sub_begin_time{0};
  uint64_t sub_end_time{0};
  bool sub_task_enable{false};
};

thread_local MixVectorProf mix_prof_data;

// call after kernel launch
std::string PrintStreamIdAndTaskId(const KernelContext *context) {
  auto stream = context->GetInputValue<void *>(static_cast<int32_t>(InputCommon::kStream));
  std::stringstream ss;
  uint32_t stream_id = 0U;
  uint32_t flip_task_id = 0U;
  if ((aclrtGetThreadLastTaskId(&flip_task_id) == RT_ERROR_NONE) && (aclrtStreamGetId(stream, reinterpret_cast<int32_t*>(&stream_id)) == ACL_SUCCESS)) {
    const uint32_t task_id = flip_task_id & 0xFFFF; // lower 16bits
    const uint32_t flip_num = flip_task_id >> 16U;   // high 16bits
    ss << "stream_id=" << stream_id << ", task_id=" << task_id << ", flip_num=" << flip_num
       << ", flip_task_id=" << flip_task_id;
  }
  return ss.str();
}

std::string PrintArgsGeneralInfo(const RtKernelLaunchArgsEx *args) {
  std::stringstream ss;
  ss << "Launch args-args addresses info(Will not pass to the launch func): ";
  ss << "args-args address base " << args->GetBase()->args << ", args-args-size " << args->GetBase()->argsSize << " ";
  for (uint32_t i = 0; i < static_cast<uint32_t>(RtKernelLaunchArgsEx::ArgsType::kArgsTypeEnd); ++i) {
    auto i_type = static_cast<RtKernelLaunchArgsEx::ArgsType>(i);
    if (i_type > RtKernelLaunchArgsEx::ArgsType::kWorkspacesAddr &&
        i_type < RtKernelLaunchArgsEx::ArgsType::kTilingData) {
      // 由于workspace的数量不固定，而条件中的字段要求连续存储，因此这部分字段的地址时不准确的，因此干脆不打了
      continue;
    }
    ss << kLaunchArgsTypeName[i]
       << "(address/length): " << PtrToPtr<uint8_t, void>(args->GetArgsPointer<uint8_t>(i_type)) << "/"
       << args->GetArgsCap(i_type) << ", ";
    if (i_type == RtKernelLaunchArgsEx::ArgsType::kArgsOutputsAddr) {
      ss << "(with " << args->GetMergedCopySize() << " tensor list behind.), ";
    }
  }
  return ss.str();
}

std::string PrintCompiledArgs(const RtKernelLaunchArgsEx *args) {
  std::stringstream ss;
  auto compiled_args_len = args->GetArgsCap(RtKernelLaunchArgsEx::ArgsType::kArgsCompiledArgs);
  if (compiled_args_len > 0) {
    ss << "Compiled Args(" << compiled_args_len << "): ";
    PrintHex(args->GetArgsPointer<uint8_t>(RtKernelLaunchArgsEx::ArgsType::kArgsCompiledArgs), compiled_args_len, ss);
  } else {
    ss << "No compiled args";
  }
  return ss.str();
}
std::string PrintIoAddresses(const RtKernelLaunchArgsEx *args) {
  std::stringstream ss;
  auto &io_args_info = args->GetIoArgsInfo();
  std::vector<TensorAddress> io_addr(io_args_info.GetIoArgNum());
  std::vector<size_t> io_sizes(io_args_info.GetIoArgNum());
  for (size_t idx = 0U; idx < io_args_info.GetIoArgNum(); ++idx) {
    auto io_arg = io_args_info.GetIoArgByIndex(idx);
    size_t io_addr_offset = io_arg->arg_offset;
    auto io_addr_addr =
        reinterpret_cast<const TensorAddress *>(static_cast<const uint8_t *>(args->GetArgBase()) + io_addr_offset);
    io_addr[idx] = *io_addr_addr;
    io_sizes[idx] = io_arg->data_size;
  }
  if (io_args_info.GetIoArgNum() > 0U) {
    ss << "Input/Output addresses: ";
    PrintHex(io_addr.data(), io_args_info.GetIoArgNum(), ss);
    ss << ", Input/Output sizes: ";
    for (const auto size : io_sizes) {
      ss << size << " ";
    }
  } else {
    ss << "No inputs/outputs";
  }
  return ss.str();
}
std::string PrintWorkspaceAddress(const KernelContext *context) {
  std::stringstream ss;
  auto workspace = context->GetInputPointer<ContinuousVector>(static_cast<int32_t>(InputCommon::kWorkspaceAddr));
  auto args = context->GetInputPointer<RtKernelLaunchArgsEx>(static_cast<size_t>(InputCommon::kRtArg));
  GE_CHECK_NOTNULL_EXEC(workspace, return ss.str());
  GE_CHECK_NOTNULL_EXEC(args, return ss.str());
  auto count = workspace->GetSize();
  auto current_address = args->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kWorkspacesAddr);
  if (count > 0) {
    ss << "Workspace addresses: ";
    PrintHex(current_address, count, ss);
  } else {
    ss << "No workspaces";
  }
  return ss.str();
}
std::string PrintArgs(const RtKernelLaunchArgsEx *args) {
  std::stringstream ss;
  PrintHex(args->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kArgsInputsAddr),
           args->GetBase()->argsSize / sizeof(TensorAddress), ss);
  return ss.str();
}
std::string PrintTiling(const RtKernelLaunchArgsEx *args) {
  std::stringstream ss;
  if (args->GetBase()->hasTiling) {
    ss << "Tiling address offset " << args->GetBase()->tilingAddrOffset;
    ss << ", Tiling data(" << args->GetTilingData().GetDataSize() << " bytes): ";
    PrintHex(PtrToPtr<void, uint8_t>(args->GetTilingData().GetData()), args->GetTilingData().GetDataSize(), ss);
  } else {
    ss << "No tiling data";
  }
  return ss.str();
}
void PrintHostData(const RtKernelLaunchArgsEx *args, std::vector<std::string> &msgs) {
  std::stringstream ss;
  ss << "Host input data: ";
  auto host_data_cap = args->GetArgsCap(RtKernelLaunchArgsEx::ArgsType::kHostInputData);
  PrintHex(args->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kHostInputData),
           host_data_cap / sizeof(TensorAddress), ss);
  msgs.emplace_back(ss.str());
  ss.str("");
  ss << "Host input info: ";
  host_data_cap = args->GetArgsCap(RtKernelLaunchArgsEx::ArgsType::kHostInputInfo);
  PrintHex(args->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kHostInputInfo),
           host_data_cap / sizeof(TensorAddress), ss);
  msgs.emplace_back(ss.str());
  return;
}
std::vector<std::string> PrintLaunchArgs(const KernelContext *context) {
  auto stream = context->GetInputValue<void *>(static_cast<int32_t>(InputCommon::kStream));
  FE_ASSERT_NOTNULL(stream);
  auto bin_handle = context->GetInputValue<aclrtBinHandle>(static_cast<int32_t>(InputCommon::kBinHandle));
  FE_ASSERT_NOTNULL(bin_handle);
  auto block_dim = context->GetInputPointer<uint32_t>(static_cast<int32_t>(InputCommon::kBlockDim));
  FE_ASSERT_NOTNULL(block_dim);
  auto schedule_mode = context->GetInputValue<uint32_t>(static_cast<int32_t>(InputCommon::kScheduleMode));
  auto local_mem_size = context->GetInputValue<uint32_t>(static_cast<int32_t>(InputCommon::kLocalMemSize));
  auto args = context->GetInputPointer<RtKernelLaunchArgsEx>(static_cast<size_t>(InputCommon::kRtArg));
  FE_ASSERT_NOTNULL(args);
  std::vector<std::string> msgs;
  std::stringstream ss;
  ss << "Launch function arguments: "
     << "BinHandle " << std::hex << bin_handle << ", block dim " << *block_dim << ", args " << args->GetBase() << ", stream "
     << stream << ", schedule mode: " << std::to_string(schedule_mode) << ", local mem size: "
     << std::to_string(local_mem_size);
  msgs.emplace_back(ss.str());
  msgs.emplace_back(PrintStreamIdAndTaskId(context));
  msgs.emplace_back(PrintArgsGeneralInfo(args));
  msgs.emplace_back(PrintCompiledArgs(args));
  msgs.emplace_back(PrintIoAddresses(args));
  msgs.emplace_back(PrintWorkspaceAddress(context));
  msgs.emplace_back(PrintTiling(args));
  PrintHostData(args, msgs);
  msgs.emplace_back(PrintArgs(args));
  return msgs;
}
struct MixLaunchArgs {
  aclrtFuncHandle funcHandle;
  uint32_t block_dim;
  rtStream_t stm;
  aclrtLaunchKernelCfg *cfgInfo;
  void *hostArgs;
  uint32_t argsSize;
  aclrtPlaceHolderInfo *placeHolderArray;
  uint32_t placeHolderNum;
};

struct MixTaskPara {
  bool need_sub_task{true};
  int64_t main_blk_dim;
  int64_t main_offset;
  int64_t sub_blk_dim;
  int64_t sub_offset;
};

struct ArgsSizeInfo
{
  ge::SmallVector<uint64_t, 8U> in_outputs_size;
  uint32_t size;
  bool is_mix;
};
}  // namespace

ge::graphStatus UpdateOverflowAddr(RtKernelLaunchArgsEx &args) {
  auto overflow_addr_cap = args.GetArgsCap(RtKernelLaunchArgsEx::ArgsType::kOverflowAddr);
  if (overflow_addr_cap >= sizeof(TensorAddress)) {
    size_t overflow_offset =
        (args.GetBase()->tilingAddrOffset / sizeof(TensorAddress)) + (args.GetBase()->hasTiling ? 1UL : 0UL);
    auto args_host_buffer = static_cast<TensorAddress *>(args.GetBase()->args);
    void *overflow_addr = nullptr;
    const auto rt_ret = aclrtCtxGetFloatOverflowAddr(&overflow_addr);
    FE_CHK_RT_RET(rt_ret);
    *args.GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kOverflowAddr) = overflow_addr;
    args_host_buffer[overflow_offset] = overflow_addr;
  }
  return ge::GRAPH_SUCCESS;
}

bool UpdateL0WorkspaceInfo(const KernelContext *context, const gert::DfxExeArg* exe_arg,
                                      ge::SmallVector<uint64_t, 8U> &in_outputs_size, uint32_t &size) {
  auto workspace = context->MutableInputPointer<ContinuousVector>(static_cast<size_t>(InputCommon::kWorkspaceAddr));
  FE_ASSERT_NOTNULL(workspace);
  size_t work_size = workspace->GetSize();
  auto work_addrs = static_cast<GertTensorData *const *>(workspace->MutableData());
  FE_ASSERT_NOTNULL(work_addrs);
  for (size_t i = 0; i < work_size; i++) {
    uint64_t w_size = work_addrs[i]->GetSize();
    if (exe_arg->need_assert && i == 0) {
      w_size = (kAssertWorkFlag << kDumpTypeBitNum) | w_size;
    }
    in_outputs_size.emplace_back(w_size);
    ++size;
  }
  return true;
}

static ge::graphStatus CalcuArgsSizeInfo(const IoArgsInfo &io_args_info, const size_t io_num,
    const KernelContext *context, const int32_t addr_start, ArgsSizeInfo &args_size_info) {
  const size_t io_arg_num = io_args_info.GetIoArgNum();
  for (size_t args_info_idx = 0U; args_info_idx < io_arg_num; ++args_info_idx) {
    auto io_arg = io_args_info.GetIoArgByIndex(args_info_idx);
    FE_ASSERT_NOTNULL(io_arg);
    if (io_arg->start_index == 0xFFFF) {
      args_size_info.is_mix = true;
      continue;
    }
    if (io_arg->start_index == -1) {
      args_size_info.in_outputs_size.emplace_back(0UL);
    } else {
      if (io_arg->folded_first) {
        uint64_t in_val = (static_cast<uint64_t>(L0DumpType::kFoldedWithDesc) << kDumpTypeBitNum) | io_arg->dyn_desc.dyn_num;
        GELOGD("Dynamic idx:%d with num %u, report val:%lx.", io_arg->start_index, io_arg->dyn_desc.dyn_num, in_val);
        args_size_info.in_outputs_size.emplace_back(in_val);
        ++args_size_info.size;
      }
      auto input_i = static_cast<size_t>(io_arg->start_index);
      FE_ASSERT_TRUE(input_i < io_num, "[PARAM][INVALID] Input index[%zu], expect in range[0, %zu).", input_i, io_num);
      auto tensor_data = context->GetInputValue<gert::TensorData *>(addr_start + input_i);
      FE_ASSERT_NOTNULL(tensor_data);
      args_size_info.in_outputs_size.emplace_back(tensor_data->GetSize());
    }
    ++args_size_info.size;
  }
  return ge::GRAPH_SUCCESS;
}

// |   64bit    |   64bit    |   64bit    |   64bit    |   64bit    |   64bit    |   64bit    |   64bit    |
// |            | offset(32b)|   64bit    | type=0(8b) | type=2(8b) | type=0(8b) | type=0(8b) | type=0(8b) |
// | atomic_idx |  num(32b)  |   64bit    |  size(56b) |dy_num(56b) |  size(56b) |  size(56b) |  size(56b) |
//                           |   offset   |
ge::graphStatus SaveAicoreL0ExceptionDump(const IoArgsInfo &io_args_info, const size_t io_num,
    const KernelContext *context, const int32_t addr_start, gert::GertTensorData *shape_buffer) {
  auto exe_arg = context->GetInputPointer<gert::DfxExeArg>(static_cast<size_t>(InputCommon::kDfxArgs));
  FE_ASSERT_NOTNULL(exe_arg);
  if (!exe_arg->need_assert) {
    return ge::GRAPH_SUCCESS;
  }
  ArgsSizeInfo args_size_info{ge::SmallVector<uint64_t, 8U>(), 0U, false};
  (void)CalcuArgsSizeInfo(io_args_info, io_num, context, addr_start, args_size_info);
  if (shape_buffer != nullptr) {
    args_size_info.in_outputs_size.emplace_back(shape_buffer->GetSize());
    ++args_size_info.size;
  }
  FE_ASSERT_TRUE(UpdateL0WorkspaceInfo(context, exe_arg, args_size_info.in_outputs_size, args_size_info.size));

  rtArgsSizeInfo_t rt_args_size{};
  auto &addr = rt_args_size.infoAddr;
  const size_t limit_adx_size = (Adx::MAX_TENSOR_NUM - Adx::ADUMP_ARGS_EXCEPTION_HEAD);
  args_size_info.size = (args_size_info.size > limit_adx_size) ? limit_adx_size : args_size_info.size;
  addr = Adx::AdumpGetSizeInfoAddr(Adx::ADUMP_ARGS_EXCEPTION_HEAD + args_size_info.size, rt_args_size.atomicIndex);
  FE_ASSERT_NOTNULL(addr);
  static_cast<uint64_t *>(addr)[0UL] = static_cast<uint64_t>(rt_args_size.atomicIndex);
  uint64_t report_size = static_cast<uint64_t>(args_size_info.size);
  if (args_size_info.is_mix) {
    report_size |= (kDumpSkipAddrNum << kDumpOffsetBitNum);
  }
  static_cast<uint64_t *>(addr)[1UL] = report_size;
  for (size_t i = 0UL; i < static_cast<size_t>(args_size_info.size); ++i) {
    GELOGD("Report info idx: %zu, val: %lx.", i, args_size_info.in_outputs_size[i]);
    static_cast<uint64_t *>(addr)[Adx::ADUMP_ARGS_EXCEPTION_HEAD + i] = args_size_info.in_outputs_size[i];
  }
  (void)rtSetExceptionExtInfo(&rt_args_size);
  return ge::GRAPH_SUCCESS;
}

static ge::graphStatus UpdateEachArgsInfo(const KernelContext *context, const gert::IoArgsInfo &io_args_info,
                                   const int32_t &addr_start, const size_t &io_num, RtKernelLaunchArgsEx &args) {
  auto io_arg_num = io_args_info.GetIoArgNum();
  auto host_addr = args.GetArgsPointer<uint8_t>(RtKernelLaunchArgsEx::ArgsType::kHostInputData);
  FE_ASSERT_NOTNULL(host_addr);
  size_t shape_offset = 0U;  // record dynamic io every group shape offset
  for (size_t args_info_idx = 0U; args_info_idx < io_arg_num; ++args_info_idx) {
    auto io_arg = io_args_info.GetIoArgByIndex(args_info_idx);
    FE_ASSERT_NOTNULL(io_arg);
    if (io_arg->start_index == 0xFFFF) {
      void *mode_addr_ptr = nullptr;
      GE_CHK_RT_RET(aclrtGetHardwareSyncAddr(&mode_addr_ptr));
      GELOGD("Mix set sync addr: [%ld].", reinterpret_cast<uint64_t>(mode_addr_ptr));
      GE_RETURN_IF_ERROR(args.SetIoAddr(io_arg->arg_offset, mode_addr_ptr));
      continue;
    }
    if (io_arg->start_index == -1) {
      FE_RETURN_IF_ERROR(args.SetIoAddr(io_arg->arg_offset, nullptr));
      continue;
    }
    auto input_i = static_cast<size_t>(io_arg->start_index);
    FE_ASSERT_TRUE(input_i < io_num, "Input index[%zu], expect in range[0, %zu).", input_i, io_num);
    auto tensor_data = context->GetInputValue<gert::TensorData *>(addr_start + input_i);
    FE_ASSERT_NOTNULL(tensor_data);
    FE_RETURN_IF_ERROR(args.SetIoAddr(io_arg->arg_offset, tensor_data->GetAddr()));
    const_cast<IoArgsInfo::IoArg *>(io_arg)->data_size = tensor_data->GetSize();
    GELOGD("Arg id[%zu] set addr[%lx] with offset[%zu]. Tensor size is [%zu].", args_info_idx, tensor_data->GetAddr(), io_arg->arg_offset, tensor_data->GetSize());
    if (io_arg->is_need_merged_copy) {
      auto io_shape = context->GetInputValue<StorageShape *>(addr_start + io_num + input_i);
      FE_ASSERT_NOTNULL(io_shape);
      FE_RETURN_IF_ERROR(SetDynShape(io_shape->GetStorageShape(), host_addr, io_arg->dyn_desc, shape_offset));
    }
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus UpdateArgs(const KernelContext *context, const int32_t addr_start, RtKernelLaunchArgsEx &args) {
  // tiling_data地址共享，tiling-data已经在前面的tiling kernel中设置好了
  auto &tiling_data = args.GetTilingData();
  FE_RETURN_IF_ERROR(args.UpdateBaseByTilingSize(tiling_data.GetDataSize()));
  FE_RETURN_IF_ERROR(args.UpdateBaseArgsSize());

  auto io_num = context->GetInputPointer<size_t>(static_cast<size_t>(InputCommon::kIoNum));
  FE_ASSERT_NOTNULL(io_num);
  auto workspace = context->MutableInputPointer<ContinuousVector>(static_cast<size_t>(InputCommon::kWorkspaceAddr));
  FE_ASSERT_NOTNULL(workspace);

  // todo 原实现中，old怎么理解？还管上次的干啥？
  auto &io_args_info = args.GetIoArgsInfo();
  FE_ASSERT_GRAPH_SUCCESS(UpdateEachArgsInfo(context, io_args_info, addr_start, *io_num, args));

  // set shape buffer addr
  auto shape_buffer =
      context->GetInputValue<gert::GertTensorData *>(static_cast<size_t>(InputCommon::kShapeBufferAddr));
  if (shape_buffer != nullptr) {
    args.SetShapebufferAddr(shape_buffer->GetAddr());
  }

  FE_ASSERT_GRAPH_SUCCESS(SaveAicoreL0ExceptionDump(io_args_info, *io_num, context, addr_start, shape_buffer));

  auto addrs_data = static_cast<GertTensorData **>(workspace->MutableData());
  FE_ASSERT_TRUE(workspace->GetSize() <= args.GetWorkspaceCap());
  for (size_t i = 0; i < workspace->GetSize(); i++) {
    args.SetWorkspaceAddr(i, addrs_data[i]->GetAddr());
  }

  // todo 的offset偏移计算中，只计算了输入输出、workspace部分，没有计算最前面的compiled-args
  size_t new_offset = args.GetArgsOffset(RtKernelLaunchArgsEx::ArgsType::kWorkspacesAddr) +
                      workspace->GetSize() * sizeof(TensorAddress);
  args.GetBase()->tilingAddrOffset = static_cast<decltype(rtArgsEx_t::tilingAddrOffset)>(new_offset);
  FE_RETURN_IF_ERROR(UpdateOverflowAddr(args));
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus UpdateAtomicArgs(const KernelContext *context, const int32_t addr_start, RtKernelLaunchArgsEx &args) {
  // todo 部分和 UpdateArgs 重复的代码可考虑提取
  auto &tiling_data = args.GetTilingData();
  FE_RETURN_IF_ERROR(args.UpdateBaseByTilingSize(tiling_data.GetDataSize()));
  FE_RETURN_IF_ERROR(args.UpdateBaseArgsSize());

  auto io_num = context->GetInputPointer<size_t>(static_cast<int32_t>(InputCommon::kIoNum));
  auto ws_indexes =
      context->GetInputPointer<TypedContinuousVector<int64_t>>(static_cast<int32_t>(WithAtomic::kWorkspaceIndex));
  if (io_num == nullptr || ws_indexes == nullptr) {
    return ge::GRAPH_FAILED;
  }
  size_t input_i = 0U;
  for (; input_i < *io_num; ++input_i) {
    auto tensor_data = context->GetInputValue<gert::GertTensorData *>(addr_start + input_i);
    FE_ASSERT_NOTNULL(tensor_data);
    auto arg_offset =
        args.GetArgsOffset(RtKernelLaunchArgsEx::ArgsType::kArgsInputsAddr) + input_i * sizeof(TensorAddress);
    FE_RETURN_IF_ERROR(args.SetIoAddr(arg_offset, tensor_data->GetAddr()));
    auto &io_args_info = args.GetIoArgsInfo();
    auto io_arg = const_cast<IoArgsInfo::IoArg *>(io_args_info.GetIoArgByIndex(input_i));
    FE_ASSERT_NOTNULL(io_arg);
    io_arg->data_size = tensor_data->GetSize();
  }
  auto clean_workspaces = context->MutableInputPointer<ContinuousVector>(static_cast<int32_t>(addr_start + input_i));
  FE_ASSERT_NOTNULL(clean_workspaces);
  auto ws_indexes_data = ws_indexes->GetData();
  auto clean_ws_addrs_data = static_cast<GertTensorData **>(clean_workspaces->MutableData());
  for (size_t i = 0U; i < ws_indexes->GetSize(); ++i) {
    auto clean_idx = static_cast<size_t>(ws_indexes_data[i]);
    FE_ASSERT_TRUE(clean_idx < clean_workspaces->GetSize());
    input_i = (*io_num) + i;
    auto arg_offset =
        args.GetArgsOffset(RtKernelLaunchArgsEx::ArgsType::kArgsInputsAddr) + input_i * sizeof(TensorAddress);
    FE_RETURN_IF_ERROR(args.SetIoAddr(arg_offset, clean_ws_addrs_data[clean_idx]->GetAddr()));
  }
  args.GetBase()->tilingAddrOffset = static_cast<decltype(rtArgsEx_t::tilingAddrOffset)>(
      ws_indexes->GetSize() * sizeof(TensorAddress) + (*io_num) * sizeof(TensorAddress));
  FE_RETURN_IF_ERROR(UpdateOverflowAddr(args));
  return ge::GRAPH_SUCCESS;
}

aclrtFuncHandle GetFuncHandle(const aclrtBinHandle &bin_handle, const uint64_t &tiling_key, const char_t *kernel_name, uint32_t with_handle_flag) {
  GE_ASSERT_NOTNULL(bin_handle);
  aclrtFuncHandle func_handle = nullptr;
  if (with_handle_flag != 0U) {
    GELOGD("Get func handle by tiling key: %lu", tiling_key);
    GE_ASSERT_RT_OK(aclrtBinaryGetFunctionByEntry(bin_handle, tiling_key, &func_handle),
    "Get func handle by tiling key [%lu] failed.", tiling_key);
  } else {
    GELOGD("Get func handle by kernel name: %s", kernel_name);
    GE_ASSERT_RT_OK(aclrtBinaryGetFunction(bin_handle, kernel_name, &func_handle),
    "Get func handle by kernel name [%s] failed.", kernel_name);
  }
  FE_ASSERT_NOTNULL(func_handle, "Failed to get func handle.");
  return func_handle;
}

ge::graphStatus AicoreHandleUpdateGeExceptionDumpInfo(const KernelContext *context,
                                                      gert::ExceptionDumpInfoWrapper &wrapper) {
  auto workspace =
      context->MutableInputPointer<ContinuousVector>(static_cast<size_t>(InputCommon::kWorkspaceAddr));
  FE_ASSERT_NOTNULL(workspace);
  // workspace
  size_t work_size = workspace->GetSize();
  auto workspace_addrs_data = static_cast<GertTensorData **>(workspace->MutableData());
  for (size_t i = 0U; i < work_size; ++i) {
    FE_ASSERT_NOTNULL(workspace_addrs_data[i]);
    uintptr_t workspace_addr = ge::PtrToValue(workspace_addrs_data[i]->GetAddr());
    int64_t addr_size = static_cast<int64_t>(workspace_addrs_data[i]->GetSize());
    GELOGD("Add workspace, index: %zu, workspace_addr: %lu, addr_size: %ld, work_size: %zu.",
           i, static_cast<uint64_t>(workspace_addr), addr_size, work_size);
    wrapper.AddWorkspace(workspace_addr, addr_size);
  }

  // args
  auto args = context->GetInputPointer<RtKernelLaunchArgsEx>(static_cast<size_t>(InputCommon::kRtArg));
  FE_ASSERT_NOTNULL(args);
  uintptr_t args_addr = reinterpret_cast<uintptr_t>(
                        args->GetArgsPointer<TensorAddress>(RtKernelLaunchArgsEx::ArgsType::kArgsInputsAddr));
  size_t args_size = static_cast<size_t>(args->GetBase()->argsSize);
  GELOGD("Add host args, args_addr:%lu, args_size:%zu.", static_cast<uint64_t>(args_addr), args_size);
  wrapper.SetHostArgs(args_addr, args_size);

  return ge::GRAPH_SUCCESS;
}

MixTaskPara CalcMixTaskParaByType(const MixLaunchArgs &launch_args, const MixCoreArgs* mix_args) {
  MixTaskPara para;
  GELOGD("Before calc mix-type[%u], blk[%ld], ai_core[%ld], all_core[%ld].", mix_args->mix_type,
         launch_args.block_dim, mix_args->ai_core_num, mix_args->all_core_num);
  if (launch_args.block_dim <= mix_args->ai_core_num) {
    para.need_sub_task = false;
    para.main_offset = 0;
    para.main_blk_dim = launch_args.block_dim;
  } else if (mix_args->mix_type == MixType::MIX_VECTOR_CORE) {
    para.main_offset = 0;
    para.main_blk_dim = (launch_args.block_dim * mix_args->ai_core_num + mix_args->ai_core_num - 1) /
                        mix_args->all_core_num;
    para.sub_offset = para.main_blk_dim;
    para.sub_blk_dim = launch_args.block_dim - para.main_blk_dim;
  } else {
    para.main_offset = 0;
    para.sub_offset = 0;
    para.main_blk_dim = launch_args.block_dim;
    para.sub_blk_dim = mix_args->vec_core_num;
  }
  mix_prof_data.main_blk_dim = para.main_blk_dim;
  mix_prof_data.sub_blk_dim = para.sub_blk_dim;
  GELOGD("After calculating sub_offset[%ld], main_blk_dim[%ld], sub_blk_dim[%ld].", para.sub_offset, para.main_blk_dim,
          para.sub_blk_dim);
  return para;
}

ge::graphStatus LaunchMixMainTask(MixLaunchArgs &launch_args, MixTaskPara &mix_para) {
  mix_prof_data.main_begin_time = MsprofSysCycleTime();
  launch_args.cfgInfo->attrs[2].value.engineType = aclrtEngineType::ACL_RT_ENGINE_TYPE_AIC;
  launch_args.cfgInfo->attrs[3].value.blockDimOffset = mix_para.main_offset;
  FE_CHK_RT_RET(aclrtLaunchKernelWithHostArgs(launch_args.funcHandle, launch_args.block_dim, launch_args.stm, 
                                            launch_args.cfgInfo, launch_args.hostArgs, launch_args.argsSize, 
                                            launch_args.placeHolderArray, launch_args.placeHolderNum));
  mix_prof_data.main_end_time = MsprofSysCycleTime();
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus ProcMixVectorCoreTask(const KernelContext *context, MixLaunchArgs launch_args,
                                      int32_t addr_start, const RtKernelLaunchArgsEx &args) {
  auto io_num = context->GetInputValue<size_t>(static_cast<size_t>(InputCommon::kIoNum));
  int32_t mix_start = addr_start + (io_num << 1);
  auto mix_args = context->GetInputPointer<MixCoreArgs>(mix_start);
  FE_ASSERT_NOTNULL(mix_args);
  auto mix_para = CalcMixTaskParaByType(launch_args, mix_args);
  if (!mix_para.need_sub_task) {
    mix_prof_data.sub_task_enable = false;
    return LaunchMixMainTask(launch_args, mix_para);
  }
  mix_prof_data.sub_task_enable = true;
  auto sub_stream = context->GetInputValue<rtStream_t>(mix_start + 1);
  FE_ASSERT_NOTNULL(sub_stream);
  rtNotify_t rt_notify1 = context->GetInputValue<rtNotify_t>(mix_start + 2);
  FE_ASSERT_NOTNULL(rt_notify1);
  rtNotify_t rt_notify2 = context->GetInputValue<rtNotify_t>(mix_start + 3);
  FE_ASSERT_NOTNULL(rt_notify2);
  auto shape_buffer =
      context->GetInputValue<gert::GertTensorData *>(static_cast<size_t>(InputCommon::kShapeBufferAddr));
  FE_CHK_RT_RET(aclrtRecordNotify(rt_notify1, launch_args.stm));
  FE_CHK_RT_RET(aclrtWaitAndResetNotify(rt_notify1, sub_stream, UINT32_MAX));
  auto &io_args_info = args.GetIoArgsInfo();
  // main
  launch_args.cfgInfo->attrs[2].value.engineType = aclrtEngineType::ACL_RT_ENGINE_TYPE_AIC;
  launch_args.cfgInfo->attrs[3].value.blockDimOffset = mix_para.main_offset;
  mix_prof_data.main_begin_time = MsprofSysCycleTime();
  FE_CHK_RT_RET(aclrtLaunchKernelWithHostArgs(launch_args.funcHandle, launch_args.block_dim, launch_args.stm, 
                                            launch_args.cfgInfo, launch_args.hostArgs, launch_args.argsSize, 
                                            launch_args.placeHolderArray, launch_args.placeHolderNum));
  mix_prof_data.main_end_time = MsprofSysCycleTime();
  FE_ASSERT_GRAPH_SUCCESS(SaveAicoreL0ExceptionDump(io_args_info, io_num, context, addr_start, shape_buffer));
  // sub
  launch_args.cfgInfo->attrs[2].value.engineType = aclrtEngineType::ACL_RT_ENGINE_TYPE_AIV;
  launch_args.cfgInfo->attrs[3].value.blockDimOffset = mix_para.sub_offset;
  mix_prof_data.sub_begin_time = MsprofSysCycleTime();
  FE_CHK_RT_RET(aclrtLaunchKernelWithHostArgs(launch_args.funcHandle, launch_args.block_dim, sub_stream, 
                                            launch_args.cfgInfo, launch_args.hostArgs, launch_args.argsSize, 
                                            launch_args.placeHolderArray, launch_args.placeHolderNum));
  mix_prof_data.sub_end_time = MsprofSysCycleTime();
  FE_CHK_RT_RET(aclrtRecordNotify(rt_notify2, sub_stream));
  FE_CHK_RT_RET(aclrtWaitAndResetNotify(rt_notify2, launch_args.stm, UINT32_MAX));
  GELOGI("Mix vector core launched main/sub block dim [%ld][%ld] successfully", mix_para.main_blk_dim, mix_para.sub_blk_dim);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus AICoreDfxPrintProc(KernelContext *context, rtStream_t stream) {
  auto exe_arg = context->GetInputPointer<gert::DfxExeArg>(static_cast<size_t>(InputCommon::kDfxArgs));
  FE_ASSERT_NOTNULL(exe_arg);
  if (exe_arg->need_print) {
    auto workspace = context->GetInputPointer<ContinuousVector>(static_cast<size_t>(InputCommon::kWorkspaceAddr));
    FE_ASSERT_NOTNULL(workspace);
    if (workspace->GetSize() == 0) {
      return ge::GRAPH_SUCCESS;
    }
    auto work_addr = reinterpret_cast<GertTensorData *const *>(workspace->GetData());
    const auto compute_node_info = static_cast<const ComputeNodeInfo *>(context->GetComputeNodeExtend());
    FE_ASSERT_NOTNULL(compute_node_info);
    Adx::AdumpPrintWorkSpace(work_addr[0]->GetAddr(), exe_arg->buffer_size, stream, compute_node_info->GetNodeType());
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus FillMixVectorProfilingInfo(const KernelContext *context, ProfilingInfoWrapper &prof_info) {
  (void)context;
  prof_info.SetMixLaunchEnable(mix_prof_data.sub_task_enable);
  prof_info.SetBlockDim(mix_prof_data.main_blk_dim, gert::NodeProfInfoType::kOriginalNode);
  prof_info.SetBlockDim(mix_prof_data.sub_blk_dim, gert::NodeProfInfoType::kMixVectorCore);
  prof_info.SetLaunchTimeStamp(mix_prof_data.main_begin_time, mix_prof_data.main_end_time,
                               gert::NodeProfInfoType::kOriginalNode);
  prof_info.SetLaunchTimeStamp(mix_prof_data.sub_begin_time, mix_prof_data.sub_end_time,
                               gert::NodeProfInfoType::kMixVectorCore);
  GELOGI("Mix vector core report profiling data success");
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus FillAtomicAiCoreProfilingInfo(const KernelContext *context, ProfilingInfoWrapper &prof_info) {
  auto block_dim = context->GetInputPointer<uint32_t>(static_cast<int32_t>(InputCommon::kBlockDim));
  FE_ASSERT_NOTNULL(block_dim);
  prof_info.SetBlockDim(*block_dim);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus AiCoreLaunchMixKernelV2(KernelContext *context) {
  LaunchCommonInputs inputs = GetLaunchCommonInputs(context);
  FE_RETURN_IF_ERROR(PrepareLaunchConfig(inputs));
  int32_t addr_start = static_cast<int32_t>(WithArgs::kIoAddrs);
  FE_RETURN_IF_ERROR(UpdateArgs(context, addr_start, *inputs.args));
  auto funcHandle = GetFuncHandle(inputs.bin_handle, inputs.tiling_key, inputs.kernel_name, inputs.with_handle_flag);
  const auto compute_node_info = static_cast<const ComputeNodeInfo *>(context->GetComputeNodeExtend());
  FE_ASSERT_NOTNULL(compute_node_info);
  PlaceHolderInfo place_holder_info = GetPlaceHolderInfo(inputs.args);
  std::vector<aclrtPlaceHolderInfo> infos(place_holder_info.place_holder_num);
  aclrtPlaceHolderInfo *place_holder_ptr = nullptr;
  if (place_holder_info.place_holder_num > 0U) {
    UpdatePlaceHolderInfo(infos, inputs.args, place_holder_info.has_tiling);
    place_holder_ptr = infos.data();
  } else {
    static aclrtPlaceHolderInfo without_info = {};
    place_holder_ptr = &without_info;
  }
  ge::Status ret = ProcMixVectorCoreTask(context,
      {funcHandle, *inputs.block_dim, inputs.stream, inputs.cfg, inputs.args->GetArgBase(),
       inputs.args->GetBase()->argsSize, place_holder_ptr, place_holder_info.place_holder_num}, addr_start, *inputs.args);
  if (ret != RT_ERROR_NONE) {
    GELOGE(ret, "LaunchMixKernelV2 failed. Node: %s.", compute_node_info->GetNodeName());
  }
  return AICoreDfxPrintProc(context, inputs.stream);
}
REGISTER_KERNEL(LaunchMixKernelV2).RunFunc(AiCoreLaunchMixKernelV2).TracePrinter(PrintLaunchArgs).
    ExceptionDumpInfoFiller(AicoreHandleUpdateGeExceptionDumpInfo).ProfilingInfoFiller(FillMixVectorProfilingInfo);

ge::graphStatus AiCoreLaunchKernelV2(KernelContext *context) {
  LaunchCommonInputs inputs = GetLaunchCommonInputs(context);
  FE_RETURN_IF_ERROR(PrepareLaunchConfig(inputs));
  int32_t addr_start = static_cast<int32_t>(WithArgs::kIoAddrs);
  FE_RETURN_IF_ERROR(UpdateArgs(context, addr_start, *inputs.args));
  auto funcHandle = GetFuncHandle(inputs.bin_handle, inputs.tiling_key, inputs.kernel_name, inputs.with_handle_flag);
  PlaceHolderInfo place_holder_info = GetPlaceHolderInfo(inputs.args);
  ge::Status ret = LaunchKernelWithPlaceHolder(funcHandle, *inputs.block_dim, inputs.stream, inputs.cfg,
      inputs.args->GetArgBase(), inputs.args->GetBase()->argsSize, inputs.args, place_holder_info);
  const auto compute_node_info = static_cast<const ComputeNodeInfo *>(context->GetComputeNodeExtend());
  FE_ASSERT_NOTNULL(compute_node_info);
  if (ret != RT_ERROR_NONE) {
    GELOGE(ret, "LaunchKernelV2 failed. Node: %s.", compute_node_info->GetNodeName());
    return ret;
  }
  return AICoreDfxPrintProc(context, inputs.stream);
}
REGISTER_KERNEL(LaunchKernelV2).RunFunc(AiCoreLaunchKernelV2).TracePrinter(PrintLaunchArgs);

ge::graphStatus AtomicAiCoreLaunchKernelV2(KernelContext *context) {
  LaunchCommonInputs inputs = GetLaunchCommonInputs(context);
  FE_RETURN_IF_ERROR(PrepareLaunchConfig(inputs));
  FE_RETURN_IF_ERROR(UpdateAtomicArgs(context, static_cast<int32_t>(WithAtomic::kIoAddrs), *inputs.args));
  auto funcHandle = GetFuncHandle(inputs.bin_handle, inputs.tiling_key, inputs.kernel_name, inputs.with_handle_flag);
  aclrtPlaceHolderInfo info = {};
  uint32_t placeHolderNum = 0;
  if (inputs.args->GetTilingDataOffset() < inputs.args->GetBase()->argsSize) {
    info.addrOffset = inputs.args->GetBase()->tilingAddrOffset;
    info.dataOffset = inputs.args->GetTilingDataOffset();
    placeHolderNum++;
  }
  auto ret = aclrtLaunchKernelWithHostArgs(funcHandle, *inputs.block_dim, inputs.stream, inputs.cfg,
      inputs.args->GetArgBase(), inputs.args->GetBase()->argsSize, &info, placeHolderNum);
  const auto compute_node_info = static_cast<const ComputeNodeInfo *>(context->GetComputeNodeExtend());
  FE_ASSERT_NOTNULL(compute_node_info);
  if (ret != RT_ERROR_NONE) {
    GELOGE(ret, "AtomicLaunchKernelV2 failed. Node: %s.", compute_node_info->GetNodeName());
    return ret;
  }
  return ge::GRAPH_SUCCESS;
}
REGISTER_KERNEL(AtomicLaunchKernelV2).RunFunc(AtomicAiCoreLaunchKernelV2).TracePrinter(PrintLaunchArgs)
.ProfilingInfoFiller(FillAtomicAiCoreProfilingInfo);
}  // namespace kernel
}  // namespace gert