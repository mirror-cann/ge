/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_FE_KERNEL_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_FE_KERNEL_TASK_CODE_BUILDER_H_

#include <set>
#include <variant>
#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "fwk_adpt_struct.h"
#include "graph/utils/args_format_desc_utils.h"
#include "framework/common/taskdown_common.h"
#include "common/om2/codegen/om2_codegen_utils.h"
#include "common/kernel_handles_manager/kernel_handle_utils.h"
#include "register/op_tiling_registry.h"
#include "common/op_tiling/tiling_dfx.h"

namespace ge {
using AicpuShapeAndType = aicpu::FWKAdapter::ShapeAndType;
using AicpuExtInfo = aicpu::FWKAdapter::ExtInfo;
constexpr int64_t kDimEndFlag = std::numeric_limits<int64_t>::min();
constexpr uint32_t kAddressLen = static_cast<uint32_t>(sizeof(uint64_t));
constexpr uint64_t kMaxTilingDataSize = 16UL * 1024UL;

struct AicoreTaskData {
  uint32_t kernel_type{0U};
  uint32_t engine_type{0U};
  uint32_t need_assert_or_printf{0U};
};

struct AicpuTaskData {
  uint32_t kernel_type{1U};
  uint32_t engine_type{0U};
};

struct KernelBuildData {
  std::vector<OpArgDesc> ordered_args;
  std::variant<AicoreTaskData, AicpuTaskData> dispatch_info;
  KernelTaskSemantic semantic{};
};

class KernelTaskCodeBuilder : public TaskCodeBuilder {
  static constexpr const char *kDispatchFuncName = "DispatchKernel";

 public:
  explicit KernelTaskCodeBuilder(AstBuildContext &ast) : TaskCodeBuilder(ast) {}

  // ── Public overrides & accessors ──
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  std::string GetFuncName() const override;
  Status RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;
  const KernelTaskSemantic &GetTaskSemantic() const {
    return build_data_.semantic;
  }

 private:
  // ── Nested types ──
  struct ArgsFormatInfo {
    std::map<size_t, std::pair<size_t, size_t>> ir_input_2_range;
    std::map<size_t, std::pair<size_t, size_t>> ir_output_2_range;
    std::vector<ArgDesc> arg_descs;
    // header for shape infos
    std::vector<std::vector<int64_t>> shape_infos;
    size_t level1_addr_cnt{0UL};
  };

  // ── Build data assembly ──
  Status AssembleBuildData();
  Status CheckTaskSupport() const;
  void AssignTaskLocalIoNames();

  // ── Launch semantic construction ──
  Status BuildLaunchSemantic(const TaskSemanticContributeContext &context);
  Status BuildLaunchConfigSemantic(const TaskSemanticContributeContext &context);
  Status BuildLaunchFuncHandleSemantic(const TaskSemanticContributeContext &context);
  static Status ResolveKernelName(const KernelTaskSemantic &semantic, const OpDescPtr &op_desc,
                                  const domi::TaskDef &task_def, std::string &kernel_name);
  std::string ResolveFuncHandleKey(const TaskSemanticContributeContext &context, const std::string &kernel_name) const;
  Status ResolveTaskAddrs(TaskSemanticContributeContext &context);

  // ── Ordered args construction ──
  void AppendOrderedArgValue(const AddrSemantic &semantic);
  Status AppendOrderedArgValueForCommon(const AddrSemantic &semantic, const uint64_t addr_offset);
  void AppendOrderedArg(const AddrSemantic &semantic);
  void AppendOrderedPlaceholder(const TaskSemanticContributeContext &context);
  void AppendOrderedCustomValue(const TaskSemanticContributeContext &context, const uint64_t custom_value);
  Status AppendOrderedInputArg(size_t input_idx);
  Status AppendOrderedOutputArg(size_t output_idx);
  Status AppendOrderedInputOutputByInstanceIndex(const ArgDesc &arg_format);
  Status AppendOrderedInputOutputRange(const ArgDesc &arg_format, const ArgsFormatInfo &args_format_holder,
                                       const TaskSemanticContributeContext &context);
  Status AppendOrderedWorkspace(const ArgDesc &arg_format);
  Status AppendOrderedArgsByFormat(const TaskSemanticContributeContext &context,
                                   const ArgsFormatInfo &args_format_holder, std::vector<ArgDesc> &dynamic_args_desc,
                                   std::vector<size_t> &level1_desc_indices);
  void AppendOrderedDescArg(const TaskSemanticContributeContext &context, const ArgDesc &arg_format,
                            std::vector<ArgDesc> &dynamic_args_desc, std::vector<size_t> &level1_desc_indices);
  void AppendOrderedFftsAddrArg(const TaskSemanticContributeContext &context);
  void AppendOrderedEventAddrArg(const TaskSemanticContributeContext &context, const ArgDesc &arg_format,
                                 uint32_t &event_addr_index);
  void AppendOrderedOverflowAddrArg(const TaskSemanticContributeContext &context);
  void AppendOrderedTilingArg(const TaskSemanticContributeContext &context);
  Status AppendShapeInfoOrderedArgs(const TaskSemanticContributeContext &context,
                                    const ArgsFormatInfo &args_format_holder,
                                    const std::vector<ArgDesc> &dynamic_args_desc,
                                    const std::vector<size_t> &level1_desc_indices);
  void HandleShapeInfoBufferArg(const AddrSemantic &addr, uint64_t &current_args_offset,
                                std::vector<OpArgDesc> &ordered_args) const;

  // ── Shape & format utilities ──
  Status UpdateShapeAndType(const GeShape &shape, AicpuShapeAndType *const shape_and_type) const;
  void AppendShapeInfo(const ge::GeShape &shape, std::vector<int64_t> &shape_info_vec) const;
  Status ParseArgsFormat(const OpDescPtr &op_desc, ArgsFormatInfo &args_format_holder) const;
  size_t GetArgsSizeByFormat(const OpDescPtr op_desc, const ArgsFormatInfo &args_format_holder) const;
  size_t GetExtraArgsSize(const OpDescPtr &op_desc, const ccKernelType kernel_type,
                          const ArgsFormatInfo &args_format_holder) const;
  std::vector<size_t> BuildMaterializedOutputIndices(const KernelTaskSemantic &kernel_semantic) const;

  // ── Tiling & DFX ──
  Status CopyTilingDataIfNeeded(const TaskSemanticContributeContext &context, const ArgsFormatInfo &args_format_holder);
  Status ConstructDfxInfo(const ge::OpDescPtr &op_desc, const optiling::OpRunInfoV2 &run_info,
                          const std::vector<ge::ArgDesc> &arg_descs, std::string &dfx_info) const;
  Status UpdateDfxArgsAndShapeSize(const OpDescPtr &op_desc,
                                   const std::vector<optiling::ArgsIndexToIoIndex> &args_idx_to_io_idx_vec,
                                   std::vector<int64_t> &args_size_vec, std::vector<int64_t> &shape_size_vec) const;
  Status GetMemCheckStartSize(const ge::OpDescPtr &op_desc, const int64_t origin_tiling_data_size,
                              int64_t &memcheck_start_size) const;

  // ── Args table & validation ──
  void InitArgsTableEntry(const TaskSemanticContributeContext &context, const uint32_t args_size);
  Status ValidateLevel1DescTargetOffsets() const;

  // ── Ordered arg values builders (per kernel type) ──
  Status BuildOrderedArgValuesForAicore(const TaskSemanticContributeContext &context,
                                        ArgsFormatInfo &args_format_holder);
  Status BuildOrderedArgValuesForAicpu(const TaskSemanticContributeContext &context);
  Status BuildOrderedArgValuesWithoutArgsFormat(const TaskSemanticContributeContext &context);
  Status BuildAicpuArgsSemantic(const TaskSemanticContributeContext &context);
  Status BuildAicpuExtInfoSemantic(const TaskSemanticContributeContext &context);

  // ── Distribution rendering ──
  FunctionDef *RenderKernelTaskDistribute() const;
  FunctionDef *RenderUpdateExtInfoSession() const;
  FunctionDef *RenderAssembleAicpuExtInfo() const;
  FunctionDef *RenderAssembleAicpuArgs() const;
  FunctionDef *RenderAicpuKernelTaskDistribute() const;
  FunctionDef *RenderGetEventIdAddr() const;
  Status InitAicpuTaskExtInfo(uint8_t *ext_info, size_t ext_info_len, const OpDescPtr op_desc,
                              int32_t &session_info_offset) const;

  // ── Dispatch helpers ──
  Status RenderDispatchAicore(const VarRef &op, const VarRef &ctx, std::vector<DeclNode *> &items);
  Status RenderDispatchAicpu(const VarRef &op, const VarRef &ctx, std::vector<DeclNode *> &items);
  std::vector<BodyItem> RenderDispatchSetup(const VarRef &op, const VarRef &ctx);
  BodyItem RenderDispatchLoop(const VarRef &op, const VarRef &ctx);
  std::vector<BodyItem> RenderDistribution(const VarRef &op, const VarRef &ctx);
  std::vector<BodyItem> RenderAicpuDispatchSetup(const VarRef &op, const VarRef &ctx);
  std::vector<BodyItem> RenderAicpuLaunchAndAssemble(const VarRef &op, const VarRef &ctx);
  std::vector<BodyItem> RenderAicpuLaunchAndReport(const VarRef &op, const VarRef &ctx);
  Arg RenderAicoreOpDefFields(const AicoreTaskData &data);
  Arg RenderAicpuOpDefFields(const AicpuTaskData &data);

  // ── Address resolution handlers (one per OpArgType) ──
  std::vector<BodyItem> HandleInputOutputArg(const VarRef &a, const VarRef &ctx);
  std::vector<BodyItem> HandleWorkspaceArg(const VarRef &a, const VarRef &ctx);
  std::vector<BodyItem> HandleLevel1DescArg(const VarRef &a, const VarRef &ctx);
  std::vector<BodyItem> HandleShapeInfoOrCustomValueArg(const VarRef &a);
  std::vector<BodyItem> HandlePlaceholderOrOptionalEmptyArg();
  std::vector<BodyItem> HandleFftsAddrArg();
  std::vector<BodyItem> HandleEventAddrArg(const VarRef &a, const VarRef &ctx);
  std::vector<BodyItem> HandleOverflowAddrArg(const VarRef &ctx);
  std::vector<BodyItem> HandleTilingArg(const VarRef &a, const VarRef &ctx);
  std::vector<BodyItem> HandleDefaultArg();

  // ── AICPU ext info parsing ──
  Status ParseExtShape(AicpuExtInfo &aicpu_ext_info, const uint32_t num_tensor, const std::string &node_name,
                       const bool all_shape, const OpDescPtr &op_desc) const;
  Status ParseExtBitmap(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const;
  Status ParseExtTopicType(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const;
  Status ParseExtAsyncWait(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const;
  Status ParseExtInfo(uint8_t *ext_info, const size_t ext_info_len, const OpDescPtr &op_desc,
                      int32_t &session_info_offset, const uint32_t num_inputs, const uint32_t num_outputs,
                      const std::string &node_name, const bool all_shape) const;

  // ── Task data factories & utilities ──
  AicoreTaskData BuildAicoreTaskData() const;
  AicpuTaskData BuildAicpuTaskData() const;
  Status GetKernelTaskMeta(const domi::TaskDef &task_def, domi::KernelContext &kernel_context, uint32_t &args_size,
                           uint32_t &kernel_type) const;
  std::string SerializeBytesToOctalString(const std::vector<uint8_t> &buffer) const;

  // ── Member variables ──
  KernelBuildData build_data_;
  std::string kernel_name_;
  std::string tiling_data_{""};
  std::vector<size_t> materialized_output_indices_;
  int32_t cust_value_var_index_{0};
  int32_t place_holder_var_index_{0};
  uint64_t current_args_offset_{0U};
  OpDispatchType::Value dispatch_type_{OpDispatchType::DISPATCH_AICORE};
  bool has_tiling_{false};
  bool op_need_print_{false};
  bool op_need_assert_or_printf_{false};
  bool is_soft_sync_op_{false};
  bool is_separately_clean_task_{false};
  bool is_blocking_aicpu_op_{false};
};

}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_FE_KERNEL_TASK_CODE_BUILDER_H_
