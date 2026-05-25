/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_KERNEL_TASK_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_KERNEL_TASK_CODE_GENERATOR_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "rts/rts_kernel.h"
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

class KernelTaskCodeBuilder : public TaskCodeBuilder {
 public:
  explicit KernelTaskCodeBuilder(AstBuildContext &ast)
      : TaskCodeBuilder(ast), cust_value_var_index_(0), place_holder_var_index_(0) {}
  Status RenderDistribution(std::vector<BodyItem> &items) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  Status Contribute(TaskSemanticContributeContext &context) override;
  const KernelTaskSemantic &GetTaskSemantic() const {
    return semantic_;
  }

  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;

 private:
  struct RenderedAddrInfo {
    std::string var_name;
    std::string tensor_var_name;
    Om2MemoryAppType mem_type{Om2MemoryAppType::kMemoryTypeFix};
    int64_t compile_state_io_addr_offset{0};
    std::vector<BodyItem> nodes;
  };

  struct ArgsFormatInfo {
    std::map<size_t, std::pair<size_t, size_t>> ir_input_2_range;
    std::map<size_t, std::pair<size_t, size_t>> ir_output_2_range;
    std::vector<ArgDesc> arg_descs;
    // header for shape infos
    std::vector<std::vector<int64_t>> shape_infos;
    size_t level1_addr_cnt{0UL};
  };
  FunctionDef *BuildKernelTaskDistribute() const;
  FunctionDef *BuildUpdateExtInfoSession() const;
  FunctionDef *BuildAssembleAicpuExtInfo() const;
  FunctionDef *BuildAssembleAicpuArgs() const;
  FunctionDef *BuildAicpuKernelTaskDistribute() const;
  FunctionDef *BuildGetEventIdAddr() const;
  Status AppendAicpuArgsCode(Arg iow_addr, const VarRef &args_var, std::vector<BodyItem> &items);
  Status GenArgsCode();
  Status InitAicpuTaskExtInfo(uint8_t *ext_info, size_t ext_info_len, const OpDescPtr op_desc,
                              int32_t &session_info_offset) const;
  Status BuildLaunchSemantic(const TaskSemanticContributeContext &context);
  static Status ResolveKernelName(const KernelTaskSemantic &semantic, const OpDescPtr &op_desc,
                                   const domi::TaskDef &task_def, std::string &kernel_name);
  Status ResolveTaskAddrs(TaskSemanticContributeContext &context);
  Status CopyTilingDataIfNeeded(const TaskSemanticContributeContext &context, const ArgsFormatInfo &args_format_holder);
  Status ConstructDfxInfo(const ge::OpDescPtr &op_desc, const optiling::OpRunInfoV2 &run_info,
                          const std::vector<ge::ArgDesc> &arg_descs, std::string &dfx_info) const;
  Status UpdateDfxArgsAndShapeSize(const OpDescPtr &op_desc,
                                   const std::vector<optiling::ArgsIndexToIoIndex> &args_idx_to_io_idx_vec,
                                   std::vector<int64_t> &args_size_vec,
                                   std::vector<int64_t> &shape_size_vec) const;
  void AppendShapeInfo(const ge::GeShape &shape, std::vector<int64_t> &shape_info_vec) const;
  Status GetMemCheckStartSize(const ge::OpDescPtr &op_desc, const int64_t origin_tiling_data_size,
                              int64_t &memcheck_start_size) const;
  void AppendOrderedArgValue(const AddrSemantic &semantic);
  Status AppendOrderedArgValueForCommon(const AddrSemantic &semantic, const uint64_t addr_offset);
  void AppendOrderedArg(const AddrSemantic &semantic);
  Status UpdateShapeAndType(const std::vector<int64_t> &dims, const DataType data_type,
                            AicpuShapeAndType &shape_and_type) const;
  Status UpdateShapeAndType(const GeShape &shape, AicpuShapeAndType *const shape_and_type) const;
  Status ParseArgsFormat(const OpDescPtr &op_desc, ArgsFormatInfo &args_format_holder) const;
  size_t GetArgsSizeByFormat(const OpDescPtr op_desc, ArgsFormatInfo &args_format_holder) const;
  size_t GetExtraArgsSize(const OpDescPtr &op_desc, const ccKernelType kernel_type,
                          const ArgsFormatInfo &args_format_holder) const;
  void InitArgsTableEntry(const TaskSemanticContributeContext &context, const uint32_t args_size);
  Status ValidateLevel1DescTargetOffsets() const;
  std::vector<size_t> BuildMaterializedOutputIndices(const KernelTaskSemantic &kernel_semantic) const;
  void AppendOrderedPlaceholder(const TaskSemanticContributeContext &context);
  void AppendOrderedCustomValue(const TaskSemanticContributeContext &context, const uint64_t custom_value);
  Status AppendOrderedInputArg(size_t input_idx);
  Status AppendOrderedOutputArg(size_t output_idx);
  Status AppendOrderedInputOutputByInstanceIndex(const ArgDesc &arg_format);
  Status AppendOrderedInputOutputRange(const ArgDesc &arg_format, const ArgsFormatInfo &args_format_holder,
                                       const TaskSemanticContributeContext &context);
  Status AppendOrderedWorkspace(const ArgDesc &arg_format);
  Status AppendOrderedArgsByFormat(const TaskSemanticContributeContext &context, const ArgsFormatInfo &args_format_holder,
                                   std::vector<ArgDesc> &dynamic_args_desc,
                                   std::vector<size_t> &level1_desc_indices);
  void AppendOrderedDescArg(const TaskSemanticContributeContext &context, const ArgDesc &arg_format,
                            std::vector<ArgDesc> &dynamic_args_desc, std::vector<size_t> &level1_desc_indices);
  void AppendOrderedFftsAddrArg(const TaskSemanticContributeContext &context);
  void AppendOrderedEventAddrArg(const TaskSemanticContributeContext &context,
                                 const ArgDesc &arg_format, uint32_t &event_addr_index);
  void AppendOrderedOverflowAddrArg(const TaskSemanticContributeContext &context);
  void AppendOrderedTilingArg(const TaskSemanticContributeContext &context);
  Status AppendShapeInfoOrderedArgs(const TaskSemanticContributeContext &context,
                                    const ArgsFormatInfo &args_format_holder,
                                    const std::vector<ArgDesc> &dynamic_args_desc,
                                    const std::vector<size_t> &level1_desc_indices);
  Status BuildOrderedArgValuesForAicore(const TaskSemanticContributeContext &context,
                                        ArgsFormatInfo &args_format_holder);
  Status BuildOrderedArgValuesForAicpu(const TaskSemanticContributeContext &context);
  Status BuildOrderedArgValuesWithoutArgsFormat(const TaskSemanticContributeContext &context);
  Status BuildAicpuArgsSemantic(const TaskSemanticContributeContext &context);
  Status BuildAicpuExtInfoSemantic(const TaskSemanticContributeContext &context);
  Status BuildAddrGenInfoFromSemantic(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForIoTensor(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForShapeInfoBuffer(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForLevel1DescPtr(const AddrSemantic &semantic,
                                                           RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForInstance(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForFftsAddr(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForEventAddr(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  Status BuildAddrGenInfoForTiling(const AddrSemantic &semantic, RenderedAddrInfo &addr_gen_info) const;
  ExprRef BuildReportLaunchedTaskCall() const;
  Status CheckTaskSupport() const;
  Status GetKernelTaskMeta(const domi::TaskDef &task_def, domi::KernelContext &kernel_context,
                           uint32_t &args_size, uint32_t &kernel_type) const;
  Expr *BuildLaunchConfigExpr(const LaunchConfigSemantic &launch_config, Arg is_data_dump = {}) const;
  VarRef AppendLaunchConfigSetup(size_t op_index, std::vector<BodyItem> &items, Arg is_data_dump = {}) const;
  std::string SerializeBytesToOctalString(const std::vector<uint8_t> &buffer) const;
  Status ParseExtShape(AicpuExtInfo &aicpu_ext_info, const uint32_t num_tensor,
    const std::string &node_name, const bool all_shape, const OpDescPtr &op_desc) const;
  Status ParseExtBitmap(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const;
  Status ParseExtTopicType(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const;
  Status ParseExtAsyncWait(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const;
  Status ParseExtInfo(uint8_t *ext_info, const size_t ext_info_len, const OpDescPtr &op_desc,
    int32_t &session_info_offset, const uint32_t num_inputs, const uint32_t num_outputs, const std::string &node_name,
    const bool all_shape) const;
  Status AppendDistributionForAicpu(const std::vector<Arg> &args_vars, std::vector<BodyItem> &items);
 private:
  std::vector<RenderedAddrInfo> args_addr_nodes_;
  int32_t cust_value_var_index_;
  int32_t place_holder_var_index_;
  std::vector<size_t> materialized_output_indices_;
  std::string kernel_name_;
  bool op_need_print_{false};
  bool is_soft_sync_op_{false};
  bool is_separately_clean_task_{false};
  bool is_blocking_aicpu_op_{false};
  uint64_t current_args_offset_{0U};
  KernelTaskSemantic semantic_;
  bool has_tiling_{false};
  std::string tiling_data_{""};
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_KERNEL_TASK_CODE_GENERATOR_H_
