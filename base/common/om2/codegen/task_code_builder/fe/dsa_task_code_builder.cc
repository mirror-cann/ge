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
}

Status DSATaskCodeBuilder::InitSqe(const domi::DSATaskDef &dsa_task) {
  dsa_sqe_.sqe_type = static_cast<uint8_t>(dsa_task.sqe_type());
  dsa_sqe_.start = static_cast<uint8_t>(dsa_task.start());
  dsa_sqe_.distribution_type = static_cast<uint8_t>(dsa_task.distribution_type());
  dsa_sqe_.data_type = static_cast<uint8_t>(dsa_task.data_type());
  dsa_sqe_.alg_type = static_cast<uint8_t>(dsa_task.alg_type());
  dsa_sqe_.param_vld_bitmap = static_cast<uint8_t>(dsa_task.input_vld());
  dsa_sqe_.param_addr_val_bitmap = static_cast<uint8_t>(dsa_task.input_value_addr_flag());

  // Seed: could be address or immediate value
  dsa_sqe_.seed_is_addr = (dsa_task.seed_value_or_ptr() == kDSASetInputAddr);
  if (!dsa_sqe_.seed_is_addr) {
    const std::string &seed_bytes = dsa_task.args().seed_value_or_addr();
    if (seed_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&dsa_sqe_.seed_value, sizeof(uint64_t), seed_bytes.c_str(), sizeof(uint64_t)));
    }
  }

  // Random count: could be address or immediate value
  dsa_sqe_.random_count_is_addr = (dsa_task.random_count_value_or_ptr() == kDSASetInputAddr);
  if (!dsa_sqe_.random_count_is_addr) {
    const std::string &count_bytes = dsa_task.args().random_count_value_or_addr();
    if (count_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&dsa_sqe_.random_count_value, sizeof(uint64_t), count_bytes.c_str(), sizeof(uint64_t)));
    }
  }

  // Input1 for HBM workspace
  dsa_sqe_.input1_is_addr = (dsa_task.input1_value_or_ptr() == kDSASetInputAddr);
  if (!dsa_sqe_.input1_is_addr) {
    const std::string &input1_bytes = dsa_task.args().input1_value_or_addr();
    if (input1_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&dsa_sqe_.input1_value, sizeof(uint64_t), input1_bytes.c_str(), sizeof(uint64_t)));
    }
  }

  // Input2 for HBM workspace
  dsa_sqe_.input2_is_addr = (dsa_task.input2_value_or_ptr() == kDSASetInputAddr);
  if (!dsa_sqe_.input2_is_addr) {
    const std::string &input2_bytes = dsa_task.args().input2_value_or_addr();
    if (input2_bytes.size() == sizeof(uint64_t)) {
      GE_ASSERT_EOK(memcpy_s(&dsa_sqe_.input2_value, sizeof(uint64_t), input2_bytes.c_str(), sizeof(uint64_t)));
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

  const uint64_t dev_size = static_cast<uint64_t>(MemSizeAlign(
      static_cast<size_t>(workspace_bytes[wks_count - 1U]), static_cast<uint32_t>(sizeof(uint64_t))));
  const uint64_t io_size = sizeof(uint64_t) * (input_addrs_.size() + output_addrs_.size());
  hbm_args_len_ = dev_size + io_size;

  (void)hbm_entry_.emplace();
  hbm_entry_->table_index = *context.next_args_table_index;
  hbm_entry_->args_size = static_cast<uint32_t>(hbm_args_len_);
  hbm_entry_->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*hbm_entry_);
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(static_cast<uint64_t>(hbm_entry_->args_size));

  for (auto &addr : input_addrs_) {
    if (addr.memory_app == MemoryAppType::kModelIo) {
      (void)io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(addr.compile_state_io_addr_offset),
                              hbm_entry_->host_offset});
    }
  }
  for (auto &addr : output_addrs_) {
    if (addr.memory_app == MemoryAppType::kModelIo) {
      (void)io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(addr.compile_state_io_addr_offset),
                              hbm_entry_->host_offset});
    }
  }
  return SUCCESS;
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
                 "[OM2][Check][Param] DSA op %s output addr size %zu != %zu",
                 header_.op_name.c_str(), output_addrs_.size(), kDSAOutputAddrSize);
  GE_ASSERT_TRUE(input_addrs_.size() >= kDSAInputAddrSize,
                 "[OM2][Check][Param] DSA op %s input addr size %zu < %zu",
                 header_.op_name.c_str(), input_addrs_.size(), kDSAInputAddrSize);
  GE_ASSERT_TRUE(!workspace_addrs_.empty(), "[OM2][Check][Param] DSA op %s has no workspace addrs",
                 header_.op_name.c_str());

  GE_ASSERT_SUCCESS(InitSqe(dsa_task));
  GE_ASSERT_SUCCESS(InitHbmArgsTable(context));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);
  GELOGI("DSA Task Codegen: op[%s], sqe_type[%u], stream_id[%u].",
         header_.op_name.c_str(), dsa_sqe_.sqe_type, header_.stream_id);
  return SUCCESS;
}

void DSATaskCodeBuilder::RenderAddrLowHigh(const ExprRef &sqe_attr_low, const ExprRef &sqe_attr_high,
                                           const std::string &addr_expr, std::vector<BodyItem> &items) const {
  (void)items.push_back(ast_.Assign(sqe_attr_low,
      ast_.StaticCast("uint32_t",
          ast_.ReinterpretCast("uintptr_t", addr_expr) & ast_.UInt(kMask32Bits))));
  (void)items.push_back(ast_.Assign(sqe_attr_high,
      ast_.StaticCast("uint32_t",
          ast_.ReinterpretCast("uintptr_t", addr_expr) >> k32Bits)));
}

void DSATaskCodeBuilder::RenderSqeAddrFields(const VarRef &sqe_var, std::vector<BodyItem> &items) {
  // 1. dsaCfgResultAddr = output_addrs_[0]
  RenderAddrLowHigh(sqe_var.Attr("dsaCfgResultAddrLow"), sqe_var.Attr("dsaCfgResultAddrHigh"),
                    "ValueToPtr(" + output_addrs_[0U].symbol_hint + ".device_address)", items);

  // 2. dsaCfgStateAddr
  std::string state_addr;
  if (workspace_addrs_.size() == kDSAWorkspaceAddrSize) {
    state_addr = workspace_addrs_[0U].symbol_hint;
  } else {
    state_addr = "ValueToPtr(" + input_addrs_[input_addrs_.size() - 1U].symbol_hint + ".device_address)";
  }
  RenderAddrLowHigh(sqe_var.Attr("dsaCfgStateAddrLow"), sqe_var.Attr("dsaCfgStateAddrHigh"),
                    state_addr, items);

  // 3. dsaCfgParamAddr
  auto param_addr_var = ast_.Var("uint64_t", "op" + std::to_string(header_.op_index) + "_dsa_param_addr");
  (void)items.push_back(ast_.VarDecl(param_addr_var,
      ast_.StaticCast("uint64_t",
          ast_.ReinterpretCast("uintptr_t",
              args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(hbm_entry_->table_index)).Arrow("dev_addr")))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dsaCfgParamAddrLow"),
      ast_.StaticCast("uint32_t", param_addr_var & ast_.UInt(kMask32Bits))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dsaCfgParamAddrHigh"),
      ast_.StaticCast("uint32_t", param_addr_var >> k32Bits)));
}

void DSATaskCodeBuilder::RenderSqeVarFields(const VarRef &sqe_var, std::vector<BodyItem> &items) {
  // 4. dsaCfgSeed
  auto seed_var = ast_.Var("uint64_t", "op" + std::to_string(header_.op_index) + "_dsa_seed");
  if (dsa_sqe_.seed_is_addr) {
    (void)items.push_back(ast_.VarDecl(seed_var, ast_.Var("auto", input_addrs_[1U].symbol_hint).Attr("device_address")));
  } else {
    (void)items.push_back(ast_.VarDecl(seed_var, ast_.UInt(dsa_sqe_.seed_value)));
  }
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dsaCfgSeedLow"),
      ast_.StaticCast("uint32_t", seed_var & ast_.UInt(kMask32Bits))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dsaCfgSeedHigh"),
      ast_.StaticCast("uint32_t", seed_var >> k32Bits)));

  // 5. dsaCfgNumber
  auto count_var = ast_.Var("uint64_t", "op" + std::to_string(header_.op_index) + "_dsa_count");
  if (dsa_sqe_.random_count_is_addr) {
    (void)items.push_back(ast_.VarDecl(count_var, ast_.Var("auto", input_addrs_[0U].symbol_hint).Attr("device_address")));
  } else {
    (void)items.push_back(ast_.VarDecl(count_var, ast_.UInt(dsa_sqe_.random_count_value)));
  }
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dsaCfgNumberLow"),
      ast_.StaticCast("uint32_t", count_var & ast_.UInt(kMask32Bits))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dsaCfgNumberHigh"),
      ast_.StaticCast("uint32_t", count_var >> k32Bits)));
}

void DSATaskCodeBuilder::BuildIoArgsVars(std::vector<Arg> &io_args_vars, std::vector<BodyItem> &items) {
  // input1/input2
  if (dsa_sqe_.input1_is_addr) {
    (void)io_args_vars.emplace_back(input_addrs_[kDSAInput1AddrIndex].symbol_hint);
    if ((input_addrs_.size() == kDSAStateInputAddrSize) ||
        ((input_addrs_.size() == kDSAArgsInputAddrSize) && (workspace_addrs_.size() == kDSAWorkspaceAddrSize))) {
      (void)io_args_vars.emplace_back(input_addrs_[kDSAInput2AddrIndex].symbol_hint);
    }
  } else {
    auto input1_var = ast_.Var("uint64_t", "op" + std::to_string(header_.op_index) + "_dsa_input1");
    (void)items.push_back(ast_.VarDecl(input1_var, ast_.UInt(dsa_sqe_.input1_value)));
    (void)io_args_vars.emplace_back(input1_var);
    auto input2_var = ast_.Var("uint64_t", "op" + std::to_string(header_.op_index) + "_dsa_input2");
    if ((input_addrs_.size() == kDSAStateInputAddrSize) ||
        ((input_addrs_.size() == kDSAArgsInputAddrSize) && (workspace_addrs_.size() == kDSAWorkspaceAddrSize))) {
      (void)items.push_back(ast_.VarDecl(input2_var, ast_.UInt(dsa_sqe_.input2_value)));
    } else {
      (void)items.push_back(ast_.VarDecl(input2_var, ast_.UInt(0U)));
    }
    (void)io_args_vars.emplace_back(input2_var);
  }
  // input/output addrs
  for (auto &addr : input_addrs_) {
    (void)io_args_vars.emplace_back(addr.symbol_hint);
  }
  for (auto &addr : output_addrs_) {
    (void)io_args_vars.emplace_back(addr.symbol_hint);
  }
}

void DSATaskCodeBuilder::RenderHbmArgsCopy(const VarRef &sqe_var, std::vector<BodyItem> &items) {
  std::vector<Arg> io_args_vars;
  BuildIoArgsVars(io_args_vars, items);

  auto ioaddr_var = ast_.Var("std::vector<uint64_t>",
                              "op" + std::to_string(header_.op_index) + "_dsa_iow_addr");
  (void)items.emplace_back(ast_.VarDecl(ioaddr_var, FlattenHostArgs(io_args_vars)));

  (void)items.push_back(ChkStatus(MemcpyS(
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(hbm_entry_->table_index)).Arrow("host_addr"),
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(hbm_entry_->table_index)).Arrow("size"),
      ioaddr_var.Data(), ioaddr_var.Size() * ast_.Sizeof("uint64_t"))));

  (void)items.push_back(ChkStatus(AclrtMemcpy(
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(hbm_entry_->table_index)).Arrow("dev_addr"),
      hbm_entry_->args_size,
      ioaddr_var.Data(), ioaddr_var.Size() * ast_.Sizeof("uint64_t"),
      "ACL_MEMCPY_HOST_TO_DEVICE")));

  (void)items.push_back(ChkRt(RtSetTaskTag(ast_.Str(header_.op_name))));

  // 根据GetIsDataDump结果设置dump flag: true -> 2, false -> 0
  const std::string var_prefix = "op" + std::to_string(header_.op_index) + "_dsa_";
  auto is_data_dump_var = ast_.Var("const uint8_t", var_prefix + "is_data_dump");
  (void)items.emplace_back(ast_.VarDecl(is_data_dump_var,
      ast_.Call("GetIsDataDump", {Arg::StringLiteral(header_.op_name), model_id_, instance_handle_})));
  auto dump_flag_var = ast_.Var("const uint32_t", var_prefix + "dump_flag");
  (void)items.emplace_back(ast_.VarDecl(dump_flag_var,
      is_data_dump_var * ast_.UInt(2U)));

  (void)items.push_back(ChkStatus(ast_.Call("KernelDsaTaskDistribute", {
      sqe_var.Ref().Addr(),
      ast_.StaticCast("uint32_t", ast_.Sizeof("rtStarsDsaSqe_t")),
      stream_list_[static_cast<int32_t>(header_.stream_id)],
      dump_flag_var})));
  (void)TaskCodeBuilderUtil::AppendReportLaunchedTaskCall(
      ast_, items, "op" + std::to_string(header_.op_index) + "_dsa", header_,
      hbm_entry_.has_value() ? &(*hbm_entry_) : nullptr, input_addrs_, output_addrs_, workspace_addrs_,
      ModelTaskType::MODEL_TASK_DSA, 0U, stream_list_[static_cast<int64_t>(header_.stream_id)], model_id_,
      instance_handle_, args_table_, false, true);
}

Status DSATaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  (void)items.push_back(ast_.Comment("============================= " + header_.op_name
                               + " ==============================="));

  for (auto &addr : input_addrs_) {
    GE_ASSERT_TRUE(addr.tensor_info.has_value(), "[OM2] DSA input tensor info is required for %s.",
                   addr.symbol_hint.c_str());
    const auto &tensor_info = *addr.tensor_info;
    const std::string shape_var_name = addr.symbol_hint + "_shape";
    (void)items.push_back(
        ast_.VarDecl("std::vector<int64_t>", shape_var_name, ast_.InitList(ConvertToArgs(tensor_info.shape_dims))));
    const auto device_addr = (addr.kind == AddrValueKind::kConstTensor && addr.const_index.has_value())
                                 ? Arg(constants_[static_cast<int64_t>(*addr.const_index)])
                                 : Arg(GetAddr(total_dev_mem_ptr_, addr.mem_offset));
    (void)items.push_back(ast_.VarDecl("Om2Tensor", addr.symbol_hint, ast_.Call("BuildOm2Tensor", {
        device_addr,
        ast_.ULong(tensor_info.size),
        tensor_info.data_type,
        tensor_info.format,
        ast_.Var("std::vector<int64_t>", shape_var_name).Data(),
        ast_.Var("std::vector<int64_t>", shape_var_name).Size()})));
  }
  for (auto &addr : output_addrs_) {
    GE_ASSERT_TRUE(addr.tensor_info.has_value(), "[OM2] DSA output tensor info is required for %s.",
                   addr.symbol_hint.c_str());
    const auto &tensor_info = *addr.tensor_info;
    const std::string shape_var_name = addr.symbol_hint + "_shape";
    (void)items.push_back(
        ast_.VarDecl("std::vector<int64_t>", shape_var_name, ast_.InitList(ConvertToArgs(tensor_info.shape_dims))));
    (void)items.push_back(ast_.VarDecl("Om2Tensor", addr.symbol_hint, ast_.Call("BuildOm2Tensor", {
        GetAddr(total_dev_mem_ptr_, addr.mem_offset),
        ast_.ULong(tensor_info.size),
        tensor_info.data_type,
        tensor_info.format,
        ast_.Var("std::vector<int64_t>", shape_var_name).Data(),
        ast_.Var("std::vector<int64_t>", shape_var_name).Size()})));
  }
  for (auto &addr : workspace_addrs_) {
    if (!addr.is_reused_from_upstream) {
      auto &base_ptr = (addr.memory_type == (kSessionScopeMemoryMask | RT_MEMORY_HBM))
                           ? session_scope_mem_ptr_ : total_dev_mem_ptr_;
      (void)items.push_back(ast_.VarDecl("auto", addr.symbol_hint,
                                   GetAddr(base_ptr, addr.mem_offset)));
    }
  }

  auto sqe_var = ast_.Var("rtStarsDsaSqe_t", "op" + std::to_string(header_.op_index) + "_dsa_sqe");
  (void)items.push_back(ast_.VarDecl(sqe_var, ast_.InitList({})));

  (void)items.push_back(ast_.Assign(sqe_var.Attr("sqeHeader").Attr("type"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.sqe_type))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("start"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.start))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("functionType"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.distribution_type))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("dataType"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.data_type))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("algoType"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.alg_type))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("paramVldBitmap"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.param_vld_bitmap))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("paramAddrValBitmap"), ast_.UInt(static_cast<uint64_t>(dsa_sqe_.param_addr_val_bitmap))));
  (void)items.push_back(ast_.Assign(sqe_var.Attr("kernelCredit"), kDsaKernelCredit));

  RenderSqeAddrFields(sqe_var, items);
  RenderSqeVarFields(sqe_var, items);
  RenderHbmArgsCopy(sqe_var, items);

  return SUCCESS;
}

Status DSATaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto sqe = ast_.Var("const void *const", "sqe");
  auto sqe_len = ast_.Var("const uint32_t", "sqeLen");
  auto stream = ast_.Var("aclrtStream &", "stream");
  auto flag = ast_.Var("const uint32_t", "flag");

  auto inputs = ast_.Var("std::array<uintptr_t, 4>", "inputs");

  (void)items.push_back(ast_.DefineFunction("KernelDsaTaskDistribute",
      {sqe, sqe_len, stream, flag}, "aclError", {
      ast_.VarDecl(inputs, ast_.InitList({
          ast_.ReinterpretCast("uintptr_t", sqe),
          ast_.StaticCast("uintptr_t", sqe_len),
          ast_.ReinterpretCast("uintptr_t", stream),
          ast_.StaticCast("uintptr_t", flag),
      })),
      ChkRt(RtGeneralCtrl(inputs[0].Addr(),
                           ast_.StaticCast("uint32_t", kDSARtGeneralCtrlInputCnt),
                           ast_.StaticCast("uint32_t", kDSARtGeneralCtrlSubType))),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}

int64_t DSATaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::DSATaskDef &dsa_task = task_def.dsa_task();
  return static_cast<int64_t>(dsa_task.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_DSA, DSATaskCodeBuilder);
}  // namespace ge
