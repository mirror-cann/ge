/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_CODE_GENERATOR_BASE_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_CODE_GENERATOR_BASE_H_

#include <string>
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/om2_codegen_types.h"

namespace ge {
class CodeGeneratorBase {
 public:
  explicit CodeGeneratorBase(AstBuildContext &ast) : ast_(ast) {}
  virtual ~CodeGeneratorBase() = default;

 protected:
  ExprRef ChkStatus(Arg expr) const {
    return ast_.Call("OM2_CHK_STATUS", {expr});
  }
  ExprRef ChkRt(Arg expr) const {
    return ast_.Call("OM2_CHK_RT", {expr});
  }
  ExprRef ChkNotNull(Arg expr) const {
    return ast_.Call("OM2_CHK_NOTNULL", {expr});
  }
  ExprRef ChkTrue(Arg expr) const {
    return ast_.Call("OM2_CHK_TRUE", {expr});
  }
  ExprRef GetAddr(Arg mem_ptr, Arg offset) const {
    return ast_.Call("GET_ADDR", {mem_ptr, offset});
  }
  ExprRef FlattenHostArgs(std::initializer_list<Arg> args) const {
    return ast_.Call("FlattenHostArgs", args);
  }
  ExprRef FlattenHostArgs(const std::vector<Arg> &args) const {
    return ast_.Call("FlattenHostArgs", args);
  }
  ExprRef MakeGuard(const std::string &guard_name, Arg callback) const {
    return ast_.Call("OM2_MAKE_GUARD", {guard_name, callback});
  }

  ExprRef AclrtBinaryLoadFromData(Arg bin_data, Arg bin_size, Arg load_options, Arg bin_handle) const {
    return ast_.Call("aclrtBinaryLoadFromData", {bin_data, bin_size, load_options, bin_handle});
  }
  ExprRef AclrtBinaryLoadFromFile(Arg file_path, Arg load_options, Arg bin_handle) const {
    return ast_.Call("aclrtBinaryLoadFromFile", {file_path, load_options, bin_handle});
  }
  ExprRef AclrtBinaryGetFunction(Arg bin_handle, Arg kernel_name, Arg func_handle) const {
    return ast_.Call("aclrtBinaryGetFunction", {bin_handle, kernel_name, func_handle});
  }
  ExprRef AclrtBinaryGetFunctionByEntry(Arg bin_handle, Arg tiling_key, Arg func_handle) const {
    return ast_.Call("aclrtBinaryGetFunctionByEntry", {bin_handle, tiling_key, func_handle});
  }
  ExprRef AclrtRegisterCpuFunc(Arg bin_handle, Arg func_name, Arg kernel_name, Arg func_handle) const {
    return ast_.Call("aclrtRegisterCpuFunc", {bin_handle, func_name, kernel_name, func_handle});
  }
  ExprRef AclmdlRIBuildBegin(Arg model_handle, Arg priority) const {
    return ast_.Call("aclmdlRIBuildBegin", {model_handle, priority});
  }
  ExprRef AclmdlRIBuildEnd(Arg model_handle, Arg stream) const {
    return ast_.Call("aclmdlRIBuildEnd", {model_handle, stream});
  }
  ExprRef AclmdlRIDestroy(Arg model_handle) const {
    return ast_.Call("aclmdlRIDestroy", {model_handle});
  }
  ExprRef AclmdlRIBindStream(Arg model_handle, Arg stream, Arg stream_flag) const {
    return ast_.Call("aclmdlRIBindStream", {model_handle, stream, stream_flag});
  }
  ExprRef AclmdlRIUnbindStream(Arg model_handle, Arg stream) const {
    return ast_.Call("aclmdlRIUnbindStream", {model_handle, stream});
  }
  ExprRef AclmdlRIExecute(Arg model_handle, Arg timeout) const {
    return ast_.Call("aclmdlRIExecute", {model_handle, timeout});
  }
  ExprRef AclmdlRIExecuteAsync(Arg model_handle, Arg stream) const {
    return ast_.Call("aclmdlRIExecuteAsync", {model_handle, stream});
  }
  ExprRef AclrtMallocAlign32(Arg dev_ptr, Arg size, Arg policy) const {
    return ast_.Call("aclrtMallocAlign32", {dev_ptr, size, policy});
  }
  ExprRef AclrtMalloc(Arg dev_ptr, Arg size, Arg policy) const {
    return ast_.Call("aclrtMalloc", {dev_ptr, size, policy});
  }
  ExprRef AclrtMemcpy(Arg dst, Arg dest_max, Arg src, Arg count, Arg kind) const {
    return ast_.Call("aclrtMemcpy", {dst, dest_max, src, count, kind});
  }
  ExprRef AclrtMemcpyAsync(Arg dst, Arg dest_max, Arg src, Arg count, Arg kind, Arg stream) const {
    return ast_.Call("aclrtMemcpyAsync", {dst, dest_max, src, count, kind, stream});
  }
  ExprRef AclrtCreateStreamWithConfig(Arg stream, Arg priority, Arg stream_flag) const {
    return ast_.Call("aclrtCreateStreamWithConfig", {stream, priority, stream_flag});
  }
  ExprRef AclrtCreateNotify(Arg notify, Arg notify_flag) const {
    return ast_.Call("aclrtCreateNotify", {notify, notify_flag});
  }
  ExprRef AclrtCreateEventWithFlag(Arg event, Arg event_flag) const {
    return ast_.Call("aclrtCreateEventWithFlag", {event, event_flag});
  }
  ExprRef AclrtCreateLabel(Arg label) const {
    return ast_.Call("aclrtCreateLabel", {label});
  }
  ExprRef AclrtCreateLabelList(Arg label_list, Arg label_num, Arg rt_label_list) const {
    return ast_.Call("aclrtCreateLabelList", {label_list, label_num, rt_label_list});
  }
  ExprRef AclrtDestroyLabel(Arg label) const {
    return ast_.Call("aclrtDestroyLabel", {label});
  }
  ExprRef AclrtDestroyLabelList(Arg rt_label_list) const {
    return ast_.Call("aclrtDestroyLabelList", {rt_label_list});
  }
  ExprRef AclrtDestroyEvent(Arg event) const {
    return ast_.Call("aclrtDestroyEvent", {event});
  }
  ExprRef AclrtDestroyNotify(Arg notify) const {
    return ast_.Call("aclrtDestroyNotify", {notify});
  }
  ExprRef AclrtDestroyStream(Arg stream) const {
    return ast_.Call("aclrtDestroyStream", {stream});
  }
  ExprRef AclrtBinaryUnLoad(Arg bin_handle) const {
    return ast_.Call("aclrtBinaryUnLoad", {bin_handle});
  }
  ExprRef AclrtFree(Arg dev_ptr) const {
    return ast_.Call("aclrtFree", {dev_ptr});
  }
  ExprRef AclrtRecordEvent(Arg event, Arg stream) const {
    return ast_.Call("aclrtRecordEvent", {event, stream});
  }
  ExprRef AclrtResetEvent(Arg event, Arg stream) const {
    return ast_.Call("aclrtResetEvent", {event, stream});
  }
  ExprRef AclrtSetLabel(Arg label, Arg stream) const {
    return ast_.Call("aclrtSetLabel", {label, stream});
  }
  ExprRef AclrtActiveStream(Arg active_stream, Arg stream) const {
    return ast_.Call("aclrtActiveStream", {active_stream, stream});
  }
  ExprRef AclrtRecordNotify(Arg notify, Arg stream) const {
    return ast_.Call("aclrtRecordNotify", {notify, stream});
  }
  ExprRef AclrtWaitAndResetNotify(Arg notify, Arg stream, Arg timeout) const {
    return ast_.Call("aclrtWaitAndResetNotify", {notify, stream, timeout});
  }
  ExprRef RtSetTaskTag(Arg op_name) const {
    return ast_.Call("rtSetTaskTag", {op_name});
  }
  ExprRef AclrtStreamWaitEvent(Arg stream, Arg event) const {
    return ast_.Call("aclrtStreamWaitEvent", {stream, event});
  }
  ExprRef AclrtSwitchStream(Arg input_ptr, Arg cond, Arg value_ptr, Arg data_type, Arg true_stream, Arg stream) const {
    return ast_.Call("aclrtSwitchStream", {input_ptr, cond, value_ptr, data_type, true_stream, nullptr, stream});
  }
  ExprRef RtMemcpyAsyncPtr(Arg src_addr, Arg dst_max, Arg count, Arg kind, Arg stream, Arg qos_cfg) const {
    return ast_.Call("rtMemcpyAsyncPtr", {src_addr, dst_max, count, kind, stream, qos_cfg});
  }
  ExprRef RtCmoAddrTaskLaunch(Arg args_addr, Arg args_size, Arg cmo_op_code, Arg stream, Arg flag) const {
    return ast_.Call("rtCmoAddrTaskLaunch", {args_addr, args_size, cmo_op_code, stream, flag});
  }
  ExprRef RtGeneralCtrl(Arg inputs, Arg size, Arg type) const {
    return ast_.Call("rtGeneralCtrl", {inputs, size, type});
  }
  ExprRef RtStreamCreateWithFlags(Arg stream, Arg priority, Arg stream_flag) const {
    return ast_.Call("rtStreamCreateWithFlags", {stream, priority, stream_flag});
  }
  ExprRef RtKernelFusionStart(Arg stream) const {
    return ast_.Call("rtKernelFusionStart", {stream});
  }
  ExprRef RtKernelFusionEnd(Arg stream) const {
    return ast_.Call("rtKernelFusionEnd", {stream});
  }
  ExprRef AclrtMallocHelper(Arg dev_ptr, Arg size, Arg mem_type, Arg module_name) const {
    return ast_.Call("AclrtMalloc", {dev_ptr, size, mem_type, module_name});
  }
  ExprRef AclrtMemset(Arg dev_ptr, Arg max_count, Arg value, Arg count) const {
    return ast_.Call("aclrtMemset", {dev_ptr, max_count, value, count});
  }
  ExprRef AclmdlRIEndTask(Arg model_handle, Arg stream) const {
    return ast_.Call("aclmdlRIEndTask", {model_handle, stream});
  }
  ExprRef AclrtSwitchLabelByIndex(Arg ptr, Arg max_value, Arg label_list, Arg stream) const {
    return ast_.Call("aclrtSwitchLabelByIndex", {ptr, max_value, label_list, stream});
  }
  ExprRef AclrtLaunchKernelV2(Arg func_handle, Arg block_dim, Arg args, Arg args_size, Arg config, Arg stream) const {
    return ast_.Call("aclrtLaunchKernelV2", {func_handle, block_dim, args, args_size, config, stream});
  }
  ExprRef MemcpyS(Arg dst, Arg dst_max, Arg src, Arg count) const {
    return ast_.Call("memcpy_s", {dst, dst_max, src, count});
  }
  ExprRef AclrtGetHardwareSyncAddr(Arg addr) const {
    return ast_.Call("aclrtGetHardwareSyncAddr", {addr});
  };
  ExprRef AclrtCtxGetFloatOverflowAddr(Arg addr) const {
    return ast_.Call("aclrtCtxGetFloatOverflowAddr", {addr});
  }

  AstBuildContext &ast_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_CODE_GENERATOR_BASE_H_
