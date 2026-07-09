/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "dsa_task_code_builder.h"

#include <cinttypes>

#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "common/ge_common/ge_types.h"

namespace ge {
namespace {
constexpr uint32_t kMask32Bits = 0xFFFFFFFFU;
constexpr uint32_t kDsaKernelCredit = 100U;

// DSA input address layout indices
constexpr size_t kDSAInput1AddrIndex = 2U;
constexpr size_t kDSAInput2AddrIndex = 3U;

// DSA RtGeneralCtrl parameters
constexpr uint32_t kDSARtGeneralCtrlInputCnt = 4U;
constexpr uint32_t kDSARtGeneralCtrlSubType = 11U;
}  // namespace

Status DSATaskCodeBuilder::InitSqe(const domi::DSATaskDef &dsa_task) {
  build_data_.sqe_type = static_cast<uint8_t>(dsa_task.sqe_type());
  build_data_.start = static_cast<uint8_t>(dsa_task.start());
  build_data_.distribution_type = static_cast<uint8_t>(dsa_task.distribution_type());
  build_data_.data_type = static_cast<uint8_t>(dsa_task.data_type());
  build_data_.alg_type = static_cast<uint8_t>(dsa_task.alg_type());
  build_data_.param_vld_bitmap = static_cast<uint8_t>(dsa_task.input_vld());
  build_data_.param_addr_val_bitmap = static_cast<uint8_t>(dsa_task.input_value_addr_flag());

  // Seed: could be address or immediate value
  build_data_.seed_is_addr = (dsa_task.seed_value_or_ptr() == kDSASetInputAddr);
  if (!build_data_.seed_is_addr) {
    const std::string &seed_bytes = dsa_task.args().seed_value_or_addr();
    if (seed_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&build_data_.seed_value, sizeof(uint64_t), seed_bytes.c_str(), sizeof(uint64_t)));
    }
  }

  // Random count: could be address or immediate value
  build_data_.random_count_is_addr = (dsa_task.random_count_value_or_ptr() == kDSASetInputAddr);
  if (!build_data_.random_count_is_addr) {
    const std::string &count_bytes = dsa_task.args().random_count_value_or_addr();
    if (count_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&build_data_.random_count_value, sizeof(uint64_t), count_bytes.c_str(), sizeof(uint64_t)));
    }
  }

  // Input1 for HBM workspace
  build_data_.input1_is_addr = (dsa_task.input1_value_or_ptr() == kDSASetInputAddr);
  if (!build_data_.input1_is_addr) {
    const std::string &input1_bytes = dsa_task.args().input1_value_or_addr();
    if (input1_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&build_data_.input1_value, sizeof(uint64_t), input1_bytes.c_str(), sizeof(uint64_t)));
    }
  }

  // Input2 for HBM workspace
  build_data_.input2_is_addr = (dsa_task.input2_value_or_ptr() == kDSASetInputAddr);
  if (!build_data_.input2_is_addr) {
    const std::string &input2_bytes = dsa_task.args().input2_value_or_addr();
    if (input2_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&build_data_.input2_value, sizeof(uint64_t), input2_bytes.c_str(), sizeof(uint64_t)));
    }
  }
  return SUCCESS;
}

Status DSATaskCodeBuilder::InitHbmArgsTable(TaskSemanticContributeContext &context) {
  const std::vector<int64_t> workspace_bytes = context.op_desc->GetWorkspaceBytes();
  GE_ASSERT_TRUE(!workspace_bytes.empty(), "[OM2][Check][Param] DSA op %s workspace bytes is empty",
                 header_.op_name.c_str());
  const size_t wks_count = workspace_addrs_.size();
  GE_ASSERT_TRUE((wks_count > 0U) && (workspace_bytes.size() >= wks_count),
                 "[OM2][Check][Param] DSA op %s workspace addr count %zu or bytes list size %zu invalid",
                 header_.op_name.c_str(), wks_count, workspace_bytes.size());

  const uint64_t dev_size = static_cast<uint64_t>(
      MemSizeAlign(static_cast<size_t>(workspace_bytes[wks_count - 1U]), static_cast<uint32_t>(sizeof(uint64_t))));
  const uint64_t io_size = sizeof(uint64_t) * (input_addrs_.size() + output_addrs_.size());
  const uint64_t hbm_args_len = dev_size + io_size;

  (void)hbm_entry_.emplace();
  hbm_entry_->table_index = *context.next_args_table_index;
  hbm_entry_->args_size = static_cast<uint32_t>(hbm_args_len);
  hbm_entry_->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*hbm_entry_);
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(static_cast<uint64_t>(hbm_entry_->args_size));

  for (auto &addr : input_addrs_) {
    if (addr.memory_app == MemoryAppType::kModelIo) {
      (void)io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(addr.compile_state_io_addr_offset), hbm_entry_->host_offset});
    }
  }
  for (auto &addr : output_addrs_) {
    if (addr.memory_app == MemoryAppType::kModelIo) {
      (void)io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(addr.compile_state_io_addr_offset), hbm_entry_->host_offset});
    }
  }
  return SUCCESS;
}

void DSATaskCodeBuilder::InitBuildDataFields(const uint32_t task_type) {
  const uint32_t num_inputs = static_cast<uint32_t>(input_addrs_.size());
  const uint32_t num_outputs = static_cast<uint32_t>(output_addrs_.size());
  const uint32_t ws_base = num_inputs + num_outputs;
  const uint32_t state_from_ws = (workspace_addrs_.size() == kDSAWorkspaceAddrSize) ? 1U : 0U;
  const bool has_input2 =
      (input_addrs_.size() == kDSAStateInputAddrSize) ||
      ((input_addrs_.size() == kDSAArgsInputAddrSize) && (workspace_addrs_.size() == kDSAWorkspaceAddrSize));
  build_data_.op_desc_id = header_.op_desc_id;
  build_data_.sqe_size = static_cast<uint32_t>(sizeof(rtStarsDsaSqe_t));
  build_data_.stream_id = header_.stream_id;
  build_data_.dump_flag = 0U;
  build_data_.hbm_table_index = static_cast<uint32_t>(hbm_entry_.has_value() ? hbm_entry_->table_index : 0U);
  build_data_.hbm_args_size = static_cast<uint32_t>(hbm_entry_.has_value() ? hbm_entry_->args_size : 0U);
  build_data_.idx_output = num_inputs;
  build_data_.state_addr_idx = state_from_ws ? ws_base : (num_inputs - 1U);
  build_data_.idx_seed = 1U;
  build_data_.idx_count = 0U;
  build_data_.idx_input1 = kDSAInput1AddrIndex;
  build_data_.idx_input2 = kDSAInput2AddrIndex;
  build_data_.num_iov_entries = num_inputs + num_outputs;
  build_data_.state_from_workspace = (state_from_ws != 0U);
  build_data_.has_input2 = has_input2;
  build_data_.task_type = task_type;
}

Status DSATaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);

  const domi::DSATaskDef &dsa_task = context.task_def.dsa_task();

  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, input_addrs_));
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveOutputAddrs(context, false, output_addrs_));
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveWorkspaceAddrs(context, workspace_addrs_));

  GE_ASSERT_TRUE(output_addrs_.size() == kDSAOutputAddrSize,
                 "[OM2][Check][Param] DSA op %s output addr size %zu != %zu", header_.op_name.c_str(),
                 output_addrs_.size(), kDSAOutputAddrSize);
  GE_ASSERT_TRUE(input_addrs_.size() >= kDSAInputAddrSize, "[OM2][Check][Param] DSA op %s input addr size %zu < %zu",
                 header_.op_name.c_str(), input_addrs_.size(), kDSAInputAddrSize);
  GE_ASSERT_TRUE(!workspace_addrs_.empty(), "[OM2][Check][Param] DSA op %s has no workspace addrs",
                 header_.op_name.c_str());

  GE_ASSERT_SUCCESS(InitSqe(dsa_task));
  GE_ASSERT_SUCCESS(InitHbmArgsTable(context));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  GELOGI("DSA Task Codegen: op[%s], sqe_type[%u], stream_id[%u].", header_.op_name.c_str(), build_data_.sqe_type,
         header_.stream_id);
  uint64_t current_args_offset = 0U;
  auto add_addr_entries = [&](const std::vector<AddrSemantic> &addrs) {
    for (const auto &addr : addrs) {
      OpArgDesc arg = TaskCodeBuilderUtil::ConvertAddrDesc(addr);
      arg.args_offset = current_args_offset;
      current_args_offset += 8U;
      build_data_.ordered_args.push_back(std::move(arg));
    }
  };
  add_addr_entries(input_addrs_);
  add_addr_entries(output_addrs_);
  add_addr_entries(workspace_addrs_);
  InitBuildDataFields(static_cast<uint32_t>(context.task_type));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  (void)items.push_back(RenderKernelDsaTaskDistribute());
  GE_ASSERT_SUCCESS(RenderDispatchFunc(items));
  return SUCCESS;
}

FunctionDef *DSATaskCodeBuilder::RenderKernelDsaTaskDistribute() const {
  auto sqe = ast_.Var("const void *const", "sqe");
  auto sqe_len = ast_.Var("const uint32_t", "sqeLen");
  auto stream = ast_.Var("aclrtStream &", "stream");
  auto flag = ast_.Var("const uint32_t", "flag");

  auto inputs = ast_.Var("std::array<uintptr_t, 4>", "inputs");

  return ast_.DefineFunction(
      "KernelDsaTaskDistribute", {sqe, sqe_len, stream, flag}, "aclError",
      {
          ast_.VarDecl(inputs, ast_.InitList({
                                   ast_.ReinterpretCast("uintptr_t", sqe),
                                   ast_.StaticCast("uintptr_t", sqe_len),
                                   ast_.ReinterpretCast("uintptr_t", stream),
                                   ast_.StaticCast("uintptr_t", flag),
                               })),
          ChkRt(RtGeneralCtrl(inputs[0].Addr(), ast_.StaticCast("uint32_t", kDSARtGeneralCtrlInputCnt),
                              ast_.StaticCast("uint32_t", kDSARtGeneralCtrlSubType))),
          ast_.Return("ACL_SUCCESS"),
      });
}

Status DSATaskCodeBuilder::RenderDispatchFunc(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  auto dsa_data = op.Arrow("dispatch_info").Attr("dsa");
  auto addrs = ast_.Var("std::vector<uint64_t>", "addrs");
  auto sqe = ast_.Var("rtStarsDsaSqe_t", "sqe");

  GE_ASSERT_SUCCESS(RenderDispatchFuncSetup(body, ctx, dsa_data, addrs));
  GE_ASSERT_SUCCESS(RenderSqeScalars(body, dsa_data, sqe));
  GE_ASSERT_SUCCESS(RenderSqeAddrFields(body, dsa_data, ctx, sqe, addrs));
  GE_ASSERT_SUCCESS(RenderHbmIoArgs(body, dsa_data, ctx, addrs));
  GE_ASSERT_SUCCESS(RenderDispatchFuncLaunch(body, op, ctx, dsa_data, sqe));
  GE_ASSERT_SUCCESS(RenderDispatchFuncReport(body, op, ctx, dsa_data, addrs));

  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDispatchFuncSetup(std::vector<BodyItem> &body, const VarRef &ctx,
                                                   const ExprRef &dsa_data, const VarRef &addrs) {
  (void)body.push_back(ast_.Comment("=== DSA op ==="));
  (void)body.push_back(ast_.VarDecl(addrs));
  (void)body.push_back(ast_.Call("", {addrs.Attr("reserve")(dsa_data.Attr("num_args"))}));
  (void)body.push_back(ast_.For(
      ast_.VarDecl("uint32_t", "_i", ast_.UInt(0)), ast_.Var("", "_i") < dsa_data.Attr("num_args"),
      ast_.PostInc(ast_.Var("", "_i")),
      {ast_.Call("",
                 {addrs.Attr("push_back")(ast_.ReinterpretCast(
                     "uint64_t", ast_.Call("ResolveOpAddr",
                                           {dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("addr").Attr("mem_src"),
                                            dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("addr").Attr("offset"),
                                            ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"),
                                            ctx.Attr("constants")})))})}));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderSqeScalars(std::vector<BodyItem> &body, const ExprRef &dsa_data,
                                            const VarRef &dsa_sqe) {
  (void)body.push_back(ast_.VarDecl(dsa_sqe, ast_.InitList({})));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("sqeHeader").Attr("type"), dsa_data.Attr("sqe_type")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("start"), dsa_data.Attr("start")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("functionType"), dsa_data.Attr("distribution_type")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("dataType"), dsa_data.Attr("data_type")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("algoType"), dsa_data.Attr("alg_type")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("paramVldBitmap"), dsa_data.Attr("param_vld_bitmap")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("paramAddrValBitmap"), dsa_data.Attr("param_addr_val_bitmap")));
  (void)body.push_back(ast_.Assign(dsa_sqe.Attr("kernelCredit"), ast_.UInt(kDsaKernelCredit)));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderSqeAddrFields(std::vector<BodyItem> &body, const ExprRef &dsa_data, const VarRef &ctx,
                                               const VarRef &dsa_sqe, const VarRef &addrs) {
  int split_idx = 0;
  auto doSplit = [&](Arg low_attr, Arg high_attr, Arg val) {
    auto v = ast_.Var("uint64_t", "_v" + std::to_string(split_idx++));
    (void)body.push_back(ast_.VarDecl(v, val));
    (void)body.push_back(ast_.Assign(low_attr, ast_.StaticCast("uint32_t", v & ast_.UInt(kMask32Bits))));
    (void)body.push_back(ast_.Assign(high_attr, ast_.StaticCast("uint32_t", v >> ast_.UInt(32U))));
  };
  doSplit(dsa_sqe.Attr("dsaCfgResultAddrLow"), dsa_sqe.Attr("dsaCfgResultAddrHigh"),
          addrs[dsa_data.Attr("idx_output")]);
  doSplit(dsa_sqe.Attr("dsaCfgStateAddrLow"), dsa_sqe.Attr("dsaCfgStateAddrHigh"),
          addrs[dsa_data.Attr("state_addr_idx")]);
  {
    auto a = ast_.Var("ArgsInfo *", "ai");
    (void)body.push_back(ast_.VarDecl(a, ctx.Attr("args_table").Attr("GetArgsInfo")(dsa_data.Attr("hbm_table_index"))));
    doSplit(dsa_sqe.Attr("dsaCfgParamAddrLow"), dsa_sqe.Attr("dsaCfgParamAddrHigh"),
            ast_.ReinterpretCast("uint64_t", a.Arrow("dev_addr")));
  }
  {
    auto v = ast_.Var("uint64_t", "_seed");
    (void)body.push_back(ast_.VarDecl(v, dsa_data.Attr("seed_value")));
    (void)body.push_back(
        ast_.If(Arg(dsa_data.Attr("seed_is_addr")), {ast_.Assign(v, addrs[dsa_data.Attr("idx_seed")])}));
    doSplit(dsa_sqe.Attr("dsaCfgSeedLow"), dsa_sqe.Attr("dsaCfgSeedHigh"), v);
  }
  {
    auto v = ast_.Var("uint64_t", "_cnt");
    (void)body.push_back(ast_.VarDecl(v, dsa_data.Attr("random_count_value")));
    (void)body.push_back(
        ast_.If(Arg(dsa_data.Attr("random_count_is_addr")), {ast_.Assign(v, addrs[dsa_data.Attr("idx_count")])}));
    doSplit(dsa_sqe.Attr("dsaCfgNumberLow"), dsa_sqe.Attr("dsaCfgNumberHigh"), v);
  }
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderHbmIoArgs(std::vector<BodyItem> &body, const ExprRef &dsa_data, const VarRef &ctx,
                                           const VarRef &addrs) {
  auto iov = ast_.Var("std::vector<uint64_t>", "iov");
  (void)body.push_back(ast_.VarDecl(iov));
  (void)body.push_back(ast_.Call("", {iov.Attr("reserve")(dsa_data.Attr("num_args"))}));
  {
    auto in1 = ast_.Var("uint64_t", "_in1");
    auto in2 = ast_.Var("uint64_t", "_in2");
    (void)body.push_back(
        ast_.If(Arg(dsa_data.Attr("input1_is_addr")),
                {
                    ast_.VarDecl(in1, addrs[dsa_data.Attr("idx_input1")]),
                    ast_.Call("", {iov.Attr("push_back")(in1)}),
                    ast_.If(Arg(dsa_data.Attr("has_input2")),
                            {
                                ast_.VarDecl(in2, addrs[dsa_data.Attr("idx_input2")]),
                                ast_.Call("", {iov.Attr("push_back")(in2)}),
                            }),
                },
                {
                    ast_.VarDecl(in1, dsa_data.Attr("input1_value")),
                    ast_.Call("", {iov.Attr("push_back")(in1)}),
                    ast_.VarDecl(in2),
                    ast_.If(Arg(dsa_data.Attr("has_input2")), {ast_.Assign(in2, dsa_data.Attr("input2_value"))},
                            {ast_.Assign(in2, ast_.UInt(0U))}),
                    ast_.Call("", {iov.Attr("push_back")(in2)}),
                }));
  }
  (void)body.push_back(ast_.For(ast_.VarDecl("uint32_t", "_i", ast_.UInt(0U)),
                                ast_.Var("", "_i") < dsa_data.Attr("num_iov_entries"), ast_.PostInc(ast_.Var("", "_i")),
                                {ast_.Call("", {iov.Attr("push_back")(addrs[ast_.Var("", "_i")])})}));
  {
    auto a = ast_.Var("ArgsInfo *", "ai2");
    (void)body.push_back(ast_.VarDecl(a, ctx.Attr("args_table").Attr("GetArgsInfo")(dsa_data.Attr("hbm_table_index"))));
    (void)body.push_back(
        ChkStatus(MemcpyS(a.Arrow("host_addr"), a.Arrow("size"), iov.Data(), iov.Size() * ast_.Sizeof("uint64_t"))));
    (void)body.push_back(ChkStatus(AclrtMemcpy(a.Arrow("dev_addr"), a.Arrow("size"), iov.Data(),
                                               iov.Size() * ast_.Sizeof("uint64_t"), "ACL_MEMCPY_HOST_TO_DEVICE")));
  }
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDispatchFuncLaunch(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                                    const ExprRef &dsa_data, const VarRef &sqe) {
  (void)body.push_back(ChkRt(RtSetTaskTag(op.Arrow("op_name"))));
  auto dd = ast_.Var("uint8_t", "dd");
  (void)body.push_back(ast_.VarDecl(
      dd, ast_.Call("GetIsDataDump", {op.Arrow("op_name"), ctx.Attr("model_id"), ctx.Attr("instance_handle")})));
  auto df = ast_.Var("const uint32_t", "df");
  (void)body.push_back(ast_.VarDecl(df, ast_.StaticCast("uint32_t", dd) * ast_.UInt(2U)));
  (void)body.push_back(ChkStatus(
      ast_.Call("KernelDsaTaskDistribute", {sqe.Addr(), ast_.StaticCast("uint32_t", ast_.Sizeof("rtStarsDsaSqe_t")),
                                            ctx.Attr("stream_list")[dsa_data.Attr("stream_id")], df})));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDispatchFuncReport(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                                    const ExprRef &dsa_data, const VarRef &addrs) {
  auto dsa_io_tensors = ast_.Var("std::vector<Om2Tensor>", "dsa_io_tensors");
  (void)body.push_back(ast_.VarDecl(dsa_io_tensors));
  (void)body.push_back(ast_.Call("", {dsa_io_tensors.Attr("reserve")(dsa_data.Attr("num_args"))}));
  auto dsa_report_inputs = ast_.Var("std::vector<Om2TaskIoEntry>", "dsa_report_inputs");
  (void)body.push_back(ast_.VarDecl(dsa_report_inputs));
  auto dsa_report_outputs = ast_.Var("std::vector<Om2TaskIoEntry>", "dsa_report_outputs");
  (void)body.push_back(ast_.VarDecl(dsa_report_outputs));
  auto dsa_report_ws_addrs = ast_.Var("std::vector<uint64_t>", "dsa_report_ws_addrs");
  (void)body.push_back(ast_.VarDecl(dsa_report_ws_addrs));
  auto dsa_report_ws_sizes = ast_.Var("std::vector<uint64_t>", "dsa_report_ws_sizes");
  (void)body.push_back(ast_.VarDecl(dsa_report_ws_sizes));

  GE_ASSERT_SUCCESS(RenderDispatchFuncReportIo(body, dsa_data, addrs, dsa_io_tensors, dsa_report_inputs,
                                               dsa_report_outputs, dsa_report_ws_addrs, dsa_report_ws_sizes));
  GE_ASSERT_SUCCESS(RenderDispatchFuncReportSubmit(body, op, ctx, dsa_data, dsa_report_inputs, dsa_report_outputs,
                                                   dsa_report_ws_addrs, dsa_report_ws_sizes));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDispatchFuncReportIo(std::vector<BodyItem> &body, const ExprRef &dsa_data,
                                                      const VarRef &addrs, const VarRef &dsa_io_tensors,
                                                      const VarRef &dsa_report_inputs, const VarRef &dsa_report_outputs,
                                                      const VarRef &dsa_report_ws_addrs,
                                                      const VarRef &dsa_report_ws_sizes) {
  (void)body.push_back(ast_.For(
      ast_.VarDecl("uint32_t", "_i", ast_.UInt(0)), ast_.Var("", "_i") < dsa_data.Attr("num_args"),
      ast_.PostInc(ast_.Var("", "_i")),
      {
          ast_.If(
              (dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("type") == ast_.Var("", "OP_ARG_INPUT")) ||
                  (dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("type") == ast_.Var("", "OP_ARG_OUTPUT")) ||
                  (dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("type") == ast_.Var("", "OP_ARG_CONST_TENSOR")),
              {dsa_io_tensors.PushBack(ast_.Call(
                   "BuildOm2Tensor",
                   {ast_.ReinterpretCast("void *", addrs[ast_.Var("", "_i")]),
                    dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr("size"),
                    dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr("data_type"),
                    dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr("format"),
                    dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr("shape"),
                    dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr("shape_dims")})),
               ast_.VarDecl(
                   ast_.Var("Om2TaskIoEntry", "_entry"),
                   ast_.InitList({dsa_io_tensors.Attr("back")().Addr(),
                                  dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr(
                                      "args_offset")})),
               ast_.If((dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("type") == ast_.Var("", "OP_ARG_INPUT")) ||
                           (dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("type") ==
                            ast_.Var("", "OP_ARG_CONST_TENSOR")),
                       {dsa_report_inputs.PushBack(ast_.Var("", "_entry"))},
                       {dsa_report_outputs.PushBack(ast_.Var("", "_entry"))})},
              {ast_.If(dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("type") == ast_.Var("", "OP_ARG_WORKSPACE"),
                       {
                           dsa_report_ws_addrs.PushBack(addrs[ast_.Var("", "_i")]),
                           dsa_report_ws_sizes.PushBack(
                               dsa_data.Attr("args_info")[ast_.Var("", "_i")].Attr("data").Attr("tensor").Attr("size")),
                       },
                       {})}),
      }));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDispatchFuncReportSubmit(std::vector<BodyItem> &body, const VarRef &op,
                                                          const VarRef &ctx, const ExprRef &dsa_data,
                                                          const VarRef &dsa_report_inputs,
                                                          const VarRef &dsa_report_outputs,
                                                          const VarRef &dsa_report_ws_addrs,
                                                          const VarRef &dsa_report_ws_sizes) {
  auto hbm_ai = ast_.Var("ArgsInfo *", "hbm_ai");
  (void)body.push_back(
      ast_.VarDecl(hbm_ai, ctx.Attr("args_table").Attr("GetArgsInfo")(dsa_data.Attr("hbm_table_index"))));
  (void)body.push_back(ChkStatus(ast_.Call(
      "ReportLaunchedOm2Task",
      {op.Arrow("op_name"), dsa_data.Attr("op_type"), dsa_data.Attr("op_desc_id"),
       ast_.ReinterpretCast("uintptr_t", hbm_ai.Arrow("dev_addr")), dsa_data.Attr("hbm_args_size"),
       dsa_report_inputs.Data(), ast_.StaticCast("uint64_t", dsa_report_inputs.Size()), dsa_report_outputs.Data(),
       ast_.StaticCast("uint32_t", dsa_report_outputs.Size()), dsa_report_ws_addrs.Data(), dsa_report_ws_sizes.Data(),
       ast_.StaticCast("uint32_t", dsa_report_ws_sizes.Size()), dsa_data.Attr("task_type"), ast_.UInt(0U),
       ctx.Attr("stream_list")[dsa_data.Attr("stream_id")], ctx.Attr("model_id"), ctx.Attr("instance_handle"),
       ast_.UInt(1U)})));
  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  const auto &data = build_data_;
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit({{"dsa", ast_.InitList({TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args),
                                                   Arg::StringLiteral(header_.op_type),
                                                   static_cast<int64_t>(build_data_.ordered_args.size()),
                                                   data.op_desc_id,
                                                   data.sqe_type,
                                                   data.stream_id,
                                                   data.start,
                                                   data.distribution_type,
                                                   data.data_type,
                                                   data.alg_type,
                                                   data.param_vld_bitmap,
                                                   data.param_addr_val_bitmap,
                                                   static_cast<int64_t>(data.seed_value),
                                                   static_cast<int64_t>(data.seed_is_addr),
                                                   static_cast<int64_t>(data.random_count_value),
                                                   static_cast<int64_t>(data.random_count_is_addr),
                                                   static_cast<int64_t>(data.input1_value),
                                                   static_cast<int64_t>(data.input1_is_addr),
                                                   static_cast<int64_t>(data.input2_value),
                                                   data.hbm_table_index,
                                                   data.hbm_args_size,
                                                   data.idx_output,
                                                   data.state_addr_idx,
                                                   data.idx_seed,
                                                   data.idx_count,
                                                   data.idx_input1,
                                                   data.idx_input2,
                                                   data.num_iov_entries,
                                                   static_cast<int64_t>(data.has_input2),
                                                   static_cast<int64_t>(data.task_type)})}})});
  return SUCCESS;
}

std::string DSATaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

int64_t DSATaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::DSATaskDef &dsa_task = task_def.dsa_task();
  return static_cast<int64_t>(dsa_task.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_DSA, DSATaskCodeBuilder);
}  // namespace ge
