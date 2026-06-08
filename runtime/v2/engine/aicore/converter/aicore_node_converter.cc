/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aicore_node_converter.h"
#include <cinttypes>
#include <optional>
#include "common/checker.h"
#include "base/err_msg.h"
#include "graph_builder/bg_infer_shape.h"
#include "graph_builder/bg_infer_shape_range.h"
#include "graph_builder/bg_memory.h"
#include "graph_builder/bg_tiling.h"
#include "graph_builder/bg_condition.h"
#include "graph_builder/bg_platform.h"
#include "framework/common/ge_types.h"
#include "aicore_compile_results.h"
#include "exe_graph/runtime/tiling_context.h"
#include "common/hyper_status.h"
#include "runtime/rt_model.h"
#include "aicore/launch_kernel/rt_kernel_launch_args_ex.h"
#include "exe_graph/runtime/gert_tensor_data.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/ge_context.h"
#include "graph/ge_error_codes.h"
#include "register/op_tiling/op_tiling_constants.h"
#include "register/op_tiling_info.h"
#include "graph/def_types.h"
#include "exe_graph/runtime/continuous_vector.h"
#include "graph/utils/node_utils.h"
#include "lowering/lowering_utils.h"
#include "engine/aicore/fe_rt2_common.h"
#include "engine/node_converter_utils.h"
#include "engine/aicore/graph_builder/bg_aicore_memory.h"
#include "graph/load/model_manager/model_utils.h"
#include "register/graph_optimizer/fusion_common/unknown_shape_utils.h"
#include "exe_graph/lowering/value_holder_utils.h"
#include "ge/ge_api_types.h"
#include "kernel/common_kernel_impl/tiling.h"
#include "base/err_msg.h"
#include "common/opskernel/ops_kernel_info_types.h"

namespace gert {
namespace {
constexpr const size_t max_launch_cfg_num = 4UL;
constexpr size_t const AtomicTaskdefMinNum = 2;

struct ProcArgs {
  std::vector<bg::ValueHolderPtr> tiling_ret;
  std::vector<bg::ValueHolderPtr> launch_arg;
  std::vector<bg::ValueHolderPtr> ordered_holders;
  bg::ValueHolderPtr atomic_launch{nullptr};
};

struct RunCfg {
  bg::ValueHolderPtr block_dim = nullptr;
  bg::ValueHolderPtr schedule_mode = nullptr;
  bg::ValueHolderPtr local_mem_size = nullptr;
  bg::ValueHolderPtr tiling_input_launch_arg = nullptr;
  bg::ValueHolderPtr workspaces_size = nullptr;
};

struct AtomicLaunchContext {
  ge::ComputeGraphPtr tmp_graph;
  ge::NodePtr atomic_node;
  std::vector<bg::ValueHolderPtr> output_clean_sizes;
  std::vector<bg::DevMemValueHolderPtr> output_clean_addrs;
  std::unique_ptr<bg::ValueHolder::CurrentComputeNodeGuarder> current_node_guarder;
  const domi::TaskDef *task_def;
};

struct AicoreProcContext {
  bg::DevMemValueHolderPtr workspaces_addr;
  bg::DevMemValueHolderPtr shapebuffer_addr;
  std::vector<bg::ValueHolderPtr> output_sizes;
  std::vector<bg::DevMemValueHolderPtr> output_addrs;
  bg::ValueHolderPtr atomic_launch_holder;
  bg::ValueHolderPtr bin_handle;
  std::pair<bg::ValueHolderPtr, bg::ValueHolderPtr> qos_pair;
};

struct LaunchKernelParams {
  bg::ValueHolderPtr tiling_key;
  bg::ValueHolderPtr kernel_name_holder;
  bg::ValueHolderPtr with_handle_flag_holder;
};

static std::vector<bg::DevMemValueHolderPtr> LaunchAicoreKernelAndProcess(
    const ge::NodePtr &node,
    const LowerInput &lower_input,
    const AicoreProcContext &ctx,
    std::vector<bg::ValueHolderPtr> &output_shapes,
    ProcArgs &proc_arg,
    const LaunchKernelParams &params) {
  DfxExeArg dfx_exe_arg = GetOpDfxExeArg(node);
  auto dfx_holder = bg::ValueHolder::CreateConst(&dfx_exe_arg, sizeof(dfx_exe_arg));

  auto launch_holder = bg::LaunchKernelV2(
      {
        lower_input.global_data->GetStream(),
        ctx.bin_handle,
        proc_arg.tiling_ret[TilingContext::kOutputBlockDim],
        proc_arg.tiling_ret[TilingContext::kOutputScheduleMode],
        proc_arg.tiling_ret[TilingContext::kOutputLocalMemorySize],
        ctx.workspaces_addr,
        ctx.shapebuffer_addr,
        ctx.qos_pair.first,
        ctx.qos_pair.second,
        lower_input.input_shapes,
        output_shapes,
        node,
        lower_input.global_data,
        dfx_holder,
        proc_arg.tiling_ret[static_cast<size_t>(kernel::TilingExOutputIndex::kRtArg)],
        params.tiling_key,
        params.kernel_name_holder,
        params.with_handle_flag_holder,
      },
      lower_input.input_addrs, ctx.output_addrs);
  FE_ASSERT_NOTNULL(launch_holder);
  proc_arg.ordered_holders.emplace_back(launch_holder);

  auto ref_out_shapes = SetOutputShape(node, ctx.shapebuffer_addr, launch_holder, lower_input.global_data->GetStream(),
                                       output_shapes);
  if (!ref_out_shapes.empty()) {
    output_shapes = ref_out_shapes;
    proc_arg.ordered_holders.insert(proc_arg.ordered_holders.end(), ref_out_shapes.begin(), ref_out_shapes.end());
  }

  auto free_holder = bg::FreeWorkspaceMem(kOnDeviceHbm, ctx.workspaces_addr);
  if (ctx.atomic_launch_holder != nullptr) {
    bg::ValueHolder::AddDependency(ctx.atomic_launch_holder, launch_holder);
    proc_arg.atomic_launch = ctx.atomic_launch_holder;
  }
  bg::ValueHolder::AddDependency(launch_holder, free_holder);
  return ctx.output_addrs;
}

static std::unique_ptr<AtomicLaunchContext> InitAtomicLaunchContext(
    const ge::NodePtr &node,
    const LoweringGlobalData::NodeCompileResult *compile_result,
    const bg::AtomicLoweringArg &atomic_lowering_arg) {
  if (compile_result == nullptr || compile_result->task_defs.size() < AtomicTaskdefMinNum) {
    return nullptr;
  }
  ge::ComputeGraphPtr tmp_graph = nullptr;
  GE_MAKE_SHARED(tmp_graph = std::make_shared<ge::ComputeGraph>("tmp-graph"), return nullptr);
  if (tmp_graph == nullptr) {
    return nullptr;
  }
  auto ctx = std::make_unique<AtomicLaunchContext>();
  ctx->tmp_graph = tmp_graph;
  ctx->atomic_node = BuildAtomicNode(node, atomic_lowering_arg, ctx->output_clean_sizes,
                                     ctx->output_clean_addrs, tmp_graph);
  FE_ASSERT_NOTNULL(ctx->atomic_node);
  ctx->current_node_guarder = bg::ValueHolder::SetScopedCurrentComputeNode(ctx->atomic_node);
  ctx->task_def = GetTaskDef(node, compile_result, TaskDefType::kAtomicClean);
  return ctx;
}

constexpr char const *kUbOriginGraphAttrKey = "_original_fusion_graph";
constexpr char const *kMemSetAttrKey = "tbe_op_atomic_dtypes";
constexpr char const *kAtomicCoreTypeKey = "_atomic_cube_vector_core_type";
constexpr char const *kTbeOpAtomicInt64Values = "tbe_op_atomic_int64_values";
constexpr char const *kTbeOpAtomicFloatValues = "tbe_op_atomic_float_values";
constexpr char const *kRawKernelNameKey = "_kernelname";
constexpr char const *kRawAtomicKernelNameKey = "_atomic_kernelname";

bg::ValueHolderPtr CreateKernelNameHolder(const ge::NodePtr &node, const std::string &key_suffix) {
  std::string kernel_name_str;
  if (!ge::AttrUtils::GetStr(node->GetOpDesc(), node->GetName() + key_suffix, key_suffix, kernel_name_str)) {
    GELOGD("Kernel name is empty for node: %s, unable to retrieve.", node->GetName().c_str());
  }
  return bg::ValueHolder::CreateConst(kernel_name_str.c_str(), kernel_name_str.size() + 1, true);
}

bg::ValueHolderPtr CreateTilingKeyHolder() {
  uint64_t value = 0UL;
  return bg::ValueHolder::CreateConst(&value, sizeof(value), false);
}

bg::ValueHolderPtr CreateWithHandleFlagHolder(uint32_t with_handle_flag) {
  return bg::ValueHolder::CreateConst(&with_handle_flag, sizeof(with_handle_flag), false);
}

std::pair<bg::ValueHolderPtr, bg::ValueHolderPtr> PrepareQosPair(const domi::TaskDef *task_def, const ge::NodePtr &node) {
  bg::ValueHolderPtr cfg_attrs = nullptr;
  size_t actual_cfg_num = GetLaunchKernelV2Attr(cfg_attrs, task_def, node);
  bg::ValueHolderPtr qos = nullptr;
  (void)GetQosInfo(qos, actual_cfg_num);
  return {cfg_attrs, qos};
}

ge::ComputeGraphPtr GetOriginGraphFromUbNode(const ge::NodePtr &node) {
  ge::ComputeGraphPtr graph_ptr = nullptr;
  ge::AttrUtils::GetGraph(node->GetOpDesc(), kUbOriginGraphAttrKey, graph_ptr);
  return graph_ptr;
}

inline bool NodeSupportRollback(const ge::NodePtr &node) {
  if (IsThirdClassOp(node->GetOpDesc())) {
    GELOGD("Node[%s] is third class op, jump rollback aicpu.", node->GetName().c_str());
    return false;
  }
  if (!node->GetOpDesc()->HasAttr(optiling::COMPILE_INFO_JSON)) {
    GELOGD("Node [%s][%s] lacks compile info, triggering a rollback to aicpu.", node->GetNamePtr(), node->GetTypePtr());
    return false;
  }
  bool partially_supported = false;
  (void)ge::AttrUtils::GetBool(node->GetOpDesc(), ge::kPartiallySupported, partially_supported);
  return partially_supported;
}

/*
 *  If is pytorch and only have one node, do not do empty tensor
 * */

bool IsVirtualOp(const ge::OpDescPtr &op_desc) {
  if (op_desc == nullptr) {
    return false;
  }
  bool attr_no_task = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, attr_no_task);
  return attr_no_task;
}

bg::ValueHolderPtr CreateVectorHolder(const std::vector<int64_t> &workspaces) {
  size_t total_size = 0U;
  auto vec_holder = ContinuousVector::Create<int64_t>(workspaces.size(), total_size);
  FE_ASSERT_NOTNULL(vec_holder);
  auto vec = ge::PtrToPtr<uint8_t, ContinuousVector>(vec_holder.get());
  FE_ASSERT_NOTNULL(vec);
  vec->SetSize(workspaces.size());
  for (size_t i = 0U; i < workspaces.size(); ++i) {
    *(reinterpret_cast<int64_t *>(vec->MutableData()) + i) = workspaces[i];
  }
  return bg::ValueHolder::CreateConst(vec, total_size);
}

RunCfg CollectRunConfig(const ge::NodePtr &node, const LowerInput &lower_input, const domi::TaskDef *task_def,
                        std::vector<bg::ValueHolderPtr> &output_shapes, std::vector<bg::ValueHolderPtr>& launch_arg)
{
  RunCfg run_cfg;
  const auto &op_desc = node->GetOpDesc();
  bool sta_tiling_depend = false;
  run_cfg.tiling_input_launch_arg = launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)];
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_DYNAMIC_TILING_DEPEND_OP, sta_tiling_depend);
  if (op_desc->HasAttr(kStaticToDynamicSoftSyncOp) || sta_tiling_depend) {
    auto platform_info_vec = bg::AppendCoreTypeToPlatform(node, lower_input.global_data);
    if (platform_info_vec.empty()) {
      GELOGE(ge::INTERNAL_ERROR, "platform_info_vec is empty! Node: %s.", node->GetName().c_str());
      return run_cfg;
    }
    const auto platform_info = platform_info_vec[static_cast<size_t>(bg::AssemblePlatformInfoIndex::kPlatformInfo)];
    if (platform_info == nullptr) {
      GELOGE(ge::INTERNAL_ERROR, "platform_info is nullptr! Node: %s.", node->GetName().c_str());
      return run_cfg;
    }
    auto tiling_ret = bg::Tiling(
        node, lower_input.input_shapes, output_shapes,
        {platform_info, *(lower_input.global_data), launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)]});
    run_cfg.tiling_input_launch_arg = tiling_ret[static_cast<size_t>(kernel::TilingExOutputIndex::kRtArg)];
    run_cfg.block_dim = tiling_ret[TilingContext::kOutputBlockDim];
    run_cfg.schedule_mode = tiling_ret[TilingContext::kOutputScheduleMode];
    run_cfg.local_mem_size = tiling_ret[TilingContext::kOutputLocalMemorySize];
    run_cfg.workspaces_size = tiling_ret[TilingContext::kOutputWorkspace];
  } else {
    auto block_dim_value = task_def->kernel().block_dim();
    run_cfg.block_dim = bg::ValueHolder::CreateConst(&block_dim_value, sizeof(block_dim_value));
    auto schedule_mode_value = task_def->kernel().schedule_mode();
    run_cfg.schedule_mode = bg::ValueHolder::CreateConst(&schedule_mode_value, sizeof(schedule_mode_value));
    uint32_t local_mem = 0;
    (void)ge::AttrUtils::GetInt(op_desc, kLocalMemorySize, local_mem);
    run_cfg.local_mem_size = bg::ValueHolder::CreateConst(&local_mem, sizeof(local_mem));
    run_cfg.workspaces_size = CreateVectorHolder(op_desc->GetWorkspaceBytes());
  }
  return run_cfg;
}
}  // namespace

bool IsSingleOpScene(const ge::NodePtr &node) {
  const auto owner_graph = node->GetOwnerComputeGraph();
  if (owner_graph == nullptr) {
    GELOGW("Node [%s] cannot obtain the owner graph.", node->GetName().c_str());
    return false;
  }
  const auto root_graph = ge::GraphUtils::FindRootGraph(owner_graph);
  if (root_graph == nullptr) {
    GELOGE(ge::INTERNAL_ERROR, "Node[%s] can not get root graph.", node->GetName().c_str());
    return false;
  }
  bool is_single_op_scene = false;
  (void)ge::AttrUtils::GetBool(root_graph, ge::ATTR_SINGLE_OP_SCENE, is_single_op_scene);
  bool is_single_op_graph = false;
  (void)ge::AttrUtils::GetBool(owner_graph, kFESingleOpScene, is_single_op_graph);
  GELOGD("Node[%s] single op flag %d, single op graph flag %d.", node->GetName().c_str(), is_single_op_scene,
         is_single_op_graph);
  return (is_single_op_scene && is_single_op_graph);
}

bool IsAllTensorNotEmpty() {
  std::string all_tensor_not_empty_option;
  if (ge::GetContext().GetOption(ge::OPTION_ALL_TENSOR_NOT_EMPTY, all_tensor_not_empty_option) == ge::GRAPH_SUCCESS) {
    return all_tensor_not_empty_option == "1";
  }
  return false;
}

bool NeedCheckEmptyOutput(const ge::NodePtr &node) {
  if (IsAllTensorNotEmpty()) {
    return false;
  }
  size_t output_num = static_cast<size_t>(node->GetOpDesc()->GetAllOutputsDescSize());
  if (output_num == 0) {
    return false;
  }
  if (!fe::UnknownShapeUtils::IsUnknownShapeOp(*node->GetOpDesc())) {
    return false;
  }
  if (IsSingleOpScene(node)) {
    return false;
  }
  if (IsThirdClassOp(node->GetOpDesc())) {
    GELOGD("Node [%s] is a third-class operator, jumping check for empty tensor.", node->GetNamePtr());
    return false;
  }
  std::vector<std::pair<int64_t, int64_t>> shape_range;
  for (size_t i = 0; i < output_num; ++i) {
    ge::GeTensorDescPtr tensor_ptr = node->GetOpDesc()->MutableOutputDesc(i);
    if (tensor_ptr == nullptr) {
      continue;
    }
    if (tensor_ptr->GetShape().IsUnknownDimNum()) {
      GELOGD("Node[%s] output shape has -2, need check empty.", node->GetNamePtr());
      return true;
    }
    (void)tensor_ptr->GetShapeRange(shape_range);
    if (shape_range.empty()) {
      GELOGW("Node[%s/%s] has no shape range, allow check empty.", node->GetNamePtr(), node->GetTypePtr());
      return true;
    }
    for (const auto &range : shape_range) {
      if (range.first == 0) {
        GELOGD("Node [%s] has a zero start shape range; further need to check empty", node->GetNamePtr());
        return true;
      }
    }
  }
  return false;
}

bool IsThirdClassOp(const ge::OpDescPtr &op_desc) {
  if (op_desc == nullptr) {
    return false;
  }
  int32_t unknown_shape_type_val = 0;
  (void) ge::AttrUtils::GetInt(op_desc, ge::ATTR_NAME_UNKNOWN_SHAPE_TYPE, unknown_shape_type_val);
  return static_cast<ge::UnknowShapeOpType>(unknown_shape_type_val) == ge::DEPEND_SHAPE_RANGE;
}

std::vector<bg::ValueHolderPtr> SetOutputShape(const ge::NodePtr &node,
                                               const bg::DevMemValueHolderPtr &shapebuffer_addr,
                                               const bg::ValueHolderPtr &launch_arg_ref,
                                               const bg::ValueHolderPtr &stream,
                                               const std::vector<bg::ValueHolderPtr> &output_shapes) {
  if (shapebuffer_addr == nullptr || output_shapes.empty()) {
    return {};
  }
  if (!IsThirdClassOp(node->GetOpDesc())) {
    return {};
  }

  auto sync_stream = bg::ValueHolder::CreateVoid<bg::ValueHolder>("SyncStream", {stream});
  bg::ValueHolder::AddDependency(launch_arg_ref, sync_stream);

  auto ref_output_shapes =
      bg::ValueHolder::CreateDataOutput("SetOutputShape", {shapebuffer_addr}, output_shapes.size());
  if (ref_output_shapes.size() != output_shapes.size()) {
    return {};
  }
  auto shapebuffer_free_holder = bg::FreeShapeBufferMem(kOnDeviceHbm, shapebuffer_addr);
  for (size_t i = 0; i < ref_output_shapes.size(); ++i) {
    if (ref_output_shapes[i] != nullptr) {
      ref_output_shapes[i]->RefFrom(output_shapes[i]);
      bg::ValueHolder::AddDependency(sync_stream, ref_output_shapes[i]);
      bg::ValueHolder::AddDependency(ref_output_shapes[i], shapebuffer_free_holder);
    }
  }
  return ref_output_shapes;
}

ge::Status GetQosInfo(bg::ValueHolderPtr &qos, size_t actual_cfg_num) {
  aclrtLaunchKernelCfg qos_info = {};
  qos_info.numAttrs = actual_cfg_num;
  qos = bg::ValueHolder::CreateConst(&qos_info, sizeof(qos_info));
  return ge::SUCCESS;
}

bool IsVectorTask(const ge::NodePtr &node) {
  std::string core_type;
  (void)ge::AttrUtils::GetStr(node->GetOpDesc(), ge::ATTR_NAME_CUBE_VECTOR_CORE_TYPE, core_type);
  if (core_type == kCoreTypeAIV) {
    return true;
  }
  return false;
}

size_t GetLaunchKernelV2Attr(bg::ValueHolderPtr &cfg_attrs, const domi::TaskDef *task_def, const ge::NodePtr &node) {
  aclrtLaunchKernelAttr attrs[max_launch_cfg_num];
  size_t actual_cfg_num = 0UL;
  attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
  attrs[actual_cfg_num].value.schemMode = 0U;
  actual_cfg_num++;
  attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
  attrs[actual_cfg_num].value.dynUBufSize = 0U;
  actual_cfg_num++;
  attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
  attrs[actual_cfg_num].value.engineType = IsVectorTask(node) ? ACL_RT_ENGINE_TYPE_AIV : ACL_RT_ENGINE_TYPE_AIC;
  actual_cfg_num++;
  attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
  if (static_cast<ge::ModelTaskType>(task_def->type()) == ge::ModelTaskType::MODEL_TASK_ALL_KERNEL ||
      static_cast<ge::ModelTaskType>(task_def->type()) == ge::ModelTaskType::MODEL_TASK_VECTOR_ALL_KERNEL) {
    attrs[actual_cfg_num].value.blockDimOffset = task_def->kernel_with_handle().block_dim_offset();
  } else {
    attrs[actual_cfg_num].value.blockDimOffset = task_def->kernel().block_dim_offset();
  }
  actual_cfg_num++;
  cfg_attrs = bg::ValueHolder::CreateConst(&attrs, sizeof(attrs));
  return actual_cfg_num;
}

std::vector<bg::ValueHolderPtr> InferAiCoreStorageShape(const ge::NodePtr &node,
                                                        const std::vector<bg::ValueHolderPtr> &input_shapes,
                                                        LoweringGlobalData &global_data) {
  if (node == nullptr || node->GetOpDesc() == nullptr) {
    return {};
  }
  if (!fe::UnknownShapeUtils::IsUnknownShapeOp(*node->GetOpDesc())) {
    return CreateOutputShapes(node->GetOpDesc());
  }
  const auto ub_origin_graph = GetOriginGraphFromUbNode(node);
  if (ub_origin_graph != nullptr) {
    auto output_shapes = bg::InferUbGraphShape(ub_origin_graph, input_shapes, global_data);
    bg::ValueHolder::SetCurrentComputeNode(node);
    return output_shapes;
  } else if (IsThirdClassOp(node->GetOpDesc())) {
    return bg::InferMaxShape(node, input_shapes, global_data);
  } else {
    return bg::InferStorageShape(node, input_shapes, global_data);
  }
}

DfxExeArg GetOpDfxExeArg(const ge::NodePtr &node) {
  DfxExeArg dfx_exe_arg;
  dfx_exe_arg.need_print = GetDfxOptFlagByType(node, OpDfxOpt::PRINT);
  dfx_exe_arg.need_assert = GetDfxOptFlagByType(node, OpDfxOpt::ASSERT);
  (void)ge::AttrUtils::GetInt(node->GetOpDesc(), gert::kOpDfxBufferSize, dfx_exe_arg.buffer_size);
  return dfx_exe_arg;
}

static ge::Status LoadAtomicWorkspace(const ge::OpDescPtr &op_desc) {
  std::map<std::string, std::map<int64_t, int64_t>> workspace_info;
  workspace_info = op_desc->TryGetExtAttr(ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO, workspace_info);
  if (!workspace_info.empty()) {
    return ge::SUCCESS;
  }
  ge::GeAttrValue::NAMED_ATTRS workspaces;

  if (!ge::AttrUtils::GetNamedAttrs(op_desc, ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO, workspaces)) {
    return ge::SUCCESS;
  }
  std::vector<int64_t> value;
  const std::string &op_name = op_desc->GetName();
  (void)ge::AttrUtils::GetListInt(workspaces, op_name, value);
  if (value.empty()) {
    return ge::SUCCESS;
  }
  std::map<int64_t, int64_t> index_offset;
  for (size_t i = 0U; i < (value.size() - 1U); i += 2U) {  // two sets of vector, parsing the key value of the map
    index_offset[value[i]] = value[i + 1U];
  }
  workspace_info[op_name] = index_offset;
  if (!op_desc->SetExtAttr(ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO, workspace_info)) {
    GELOGE(ge::INTERNAL_ERROR, "[Set][Attr:%s]fail for node:%s.", ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO.c_str(),
           op_desc->GetName().c_str());
    REPORT_FE_ERROR("Set attr %s failed for node %s.", ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO.c_str(),
                    op_desc->GetName().c_str());
    return ge::INTERNAL_ERROR;
  }
  return ge::SUCCESS;
}

static ge::Status GetCleanIndexes(const ge::NodePtr &node, std::vector<int64_t> &clean_workspace_indexes,
                           std::vector<int64_t> &clean_output_indexes) {
  const auto &op_desc = node->GetOpDesc();
  GELOGI("Begin to do atomic optiling for op[%s, %s].", op_desc->GetNamePtr(), op_desc->GetTypePtr());
  if (LoadAtomicWorkspace(op_desc) != ge::SUCCESS) {
    GELOGE(ge::FAILED, "Failed to get workspace size list from op[%s, %s].", op_desc->GetName().c_str(),
           op_desc->GetType().c_str());
    return ge::FAILED;
  }
  (void)ge::AttrUtils::GetListInt(op_desc, ge::ATOMIC_ATTR_OUTPUT_INDEX, clean_output_indexes);
  std::map<string, std::map<int64_t, int64_t>> atomic_workspace_info;
  atomic_workspace_info = op_desc->TryGetExtAttr(ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO, atomic_workspace_info);
  if (!atomic_workspace_info.empty()) {
    const std::map<int64_t, int64_t> &workspace_idx = atomic_workspace_info[op_desc->GetName()];
    for (const auto &ws_index : workspace_idx) {
      clean_workspace_indexes.emplace_back(ws_index.first);
    }
  }
  return ge::SUCCESS;
}

ge::Status AddDataNodeForAtomic(ge::ComputeGraphPtr &graph, ge::NodePtr &clean_node, size_t output_size) {
  // add data node for workspace
  ge::OpDescPtr workspace_data_op_desc = nullptr;
  GE_MAKE_SHARED(workspace_data_op_desc = std::make_shared<ge::OpDesc>(clean_node->GetName() + "_Data_0", "Data"),
      return ge::FAILED);
  FE_ASSERT_NOTNULL(workspace_data_op_desc);
  if (workspace_data_op_desc->AddOutputDesc(ge::GeTensorDesc()) != ge::SUCCESS) {
    GELOGE(ge::FAILED, "workspace_data_op_desc add output desc failed");
    return ge::FAILED;
  }
  auto workspace_data_node = graph->AddNode(workspace_data_op_desc);
  FE_ASSERT_NOTNULL(workspace_data_node);
  auto ret = ge::GraphUtils::AddEdge(workspace_data_node->GetOutDataAnchor(0), clean_node->GetInDataAnchor(0));
  if (ret != ge::SUCCESS) {
    GELOGE(ge::FAILED, "add edge between [%s] and [%s] failed", workspace_data_node->GetName().c_str(),
           clean_node->GetName().c_str());
    return ge::FAILED;
  }

  // add data node for output
  for (size_t i = 0U; i < output_size; ++i) {
    ge::OpDescPtr data_op_desc = nullptr;
    GE_MAKE_SHARED(data_op_desc = std::make_shared<ge::OpDesc>(clean_node->GetName() + "_Data_" + std::to_string(i + 1),
                                                               "Data"), return ge::FAILED);
    FE_ASSERT_NOTNULL(data_op_desc);
    if (data_op_desc->AddOutputDesc(ge::GeTensorDesc()) != ge::SUCCESS) {
      GELOGE(ge::FAILED, "data_op_desc add output desc failed, i = %zu", i);
      return ge::FAILED;
    }
    auto data_node = graph->AddNode(data_op_desc);
    FE_ASSERT_NOTNULL(data_node);
    ret = ge::GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), clean_node->GetInDataAnchor(i + 1));
    if (ret != ge::SUCCESS) {
      GELOGE(ge::FAILED, "add edge between [%s] and [%s] failed", data_node->GetName().c_str(),
             clean_node->GetName().c_str());
      return ge::FAILED;
    }
  }
  return ge::SUCCESS;
}

ge::NodePtr BuildAtomicNode(const ge::NodePtr &origin_node, const bg::AtomicLoweringArg &atomic_lowering_arg,
                            std::vector<bg::ValueHolderPtr> &output_clean_sizes,
                            std::vector<bg::DevMemValueHolderPtr> &output_clean_addrs, ge::ComputeGraphPtr &graph) {
  GELOGD("Generate atomic logic for node %s type %s", origin_node->GetNamePtr(), origin_node->GetTypePtr());
  std::vector<int64_t> workspace_indexes, outputs_indexes;
  if (GetCleanIndexes(origin_node, workspace_indexes, outputs_indexes) != ge::SUCCESS) {
    return nullptr;
  }
  for (auto idx : outputs_indexes) {
    if (static_cast<size_t>(idx) >= atomic_lowering_arg.output_sizes.size() ||
        static_cast<size_t>(idx) >= atomic_lowering_arg.output_addrs.size()) {
      return nullptr;
    }
    output_clean_sizes.emplace_back(atomic_lowering_arg.output_sizes[idx]);
    output_clean_addrs.emplace_back(atomic_lowering_arg.output_addrs[idx]);
  }
  ge::OpDescPtr atomic_op_desc = nullptr;
  ge::OpDescPtr ori_op_desc = origin_node->GetOpDesc();
  FE_ASSERT_NOTNULL(ori_op_desc);
  if (ori_op_desc->HasAttr(kMemSetAttrKey)) {
    GE_MAKE_SHARED(atomic_op_desc = std::make_shared<ge::OpDesc>(origin_node->GetName() + "_MemSet", "MemSet"),
                                                                 return nullptr);
  } else {
    GE_MAKE_SHARED(atomic_op_desc = std::make_shared<ge::OpDesc>(origin_node->GetName() + "_AtomicClean",
                                                                 "DynamicAtomicAddrClean"), return nullptr);
  }
  if (atomic_op_desc == nullptr) {
    return nullptr;
  }
  if (!ge::AttrUtils::SetListInt(atomic_op_desc, "ClearOutIndexes", outputs_indexes)) {
    return nullptr;
  }
  atomic_op_desc->AppendIrInput("workspace", ge::kIrInputRequired);
  atomic_op_desc->AppendIrInput("output", ge::kIrInputDynamic);

  atomic_op_desc->AddInputDesc("workspace", ge::GeTensorDesc());
  for (size_t i = 0U; i < outputs_indexes.size(); ++i) {
    atomic_op_desc->AddInputDesc("output" + std::to_string(i + 1), ge::GeTensorDesc());
  }
  if (!ge::AttrUtils::SetListInt(atomic_op_desc, "WorkspaceIndexes", workspace_indexes)) {
    return nullptr;
  }
  auto clean_node = LoweringUtils::CreateEngineTaskNode(graph, atomic_op_desc, origin_node);
  if (clean_node == nullptr) {
    GELOGE(ge::FAILED, "add node failed");
    return nullptr;
  }
  auto atomic_core_type = ge::AttrUtils::GetStr(ori_op_desc, kAtomicCoreTypeKey);
  if (atomic_core_type != nullptr) {
    (void)ge::AttrUtils::SetStr(clean_node->GetOpDesc(), ge::ATTR_NAME_CUBE_VECTOR_CORE_TYPE, *atomic_core_type);
  }
  std::vector<int32_t> dtype_list;
  std::vector<int64_t> init_value_int64_list;
  std::vector<float> init_value_float_list;

  (void)ge::AttrUtils::GetListInt(ori_op_desc, kMemSetAttrKey, dtype_list);
  (void)ge::AttrUtils::GetListInt(ori_op_desc, kTbeOpAtomicInt64Values, init_value_int64_list);
  (void)ge::AttrUtils::GetListFloat(ori_op_desc, kTbeOpAtomicFloatValues, init_value_float_list);
  GELOGD("Set attr to atomic with size:%zu %zu %zu.", dtype_list.size(), init_value_int64_list.size(),
          init_value_float_list.size());
  std::vector<int64_t> mem_size_vector = {-1};
  (void)ge::AttrUtils::SetListInt(atomic_op_desc, ge::ATTR_NAME_ATOMIC_MEMSET_SIZES, mem_size_vector);
  (void)ge::AttrUtils::SetListInt(atomic_op_desc, ge::ATTR_NAME_ATOMIC_MEMSET_DTYPES, dtype_list);
  (void)ge::AttrUtils::SetListInt(atomic_op_desc, ge::ATTR_NAME_ATOMIC_MEMSET_VALUES_INT, init_value_int64_list);
  (void)ge::AttrUtils::SetListFloat(atomic_op_desc, ge::ATTR_NAME_ATOMIC_MEMSET_VALUES_FLOAT, init_value_float_list);

  atomic_op_desc->AppendIrAttrName(ge::ATTR_NAME_ATOMIC_MEMSET_SIZES);
  atomic_op_desc->AppendIrAttrName(ge::ATTR_NAME_ATOMIC_MEMSET_DTYPES);
  atomic_op_desc->AppendIrAttrName(ge::ATTR_NAME_ATOMIC_MEMSET_VALUES_INT);
  atomic_op_desc->AppendIrAttrName(ge::ATTR_NAME_ATOMIC_MEMSET_VALUES_FLOAT);

  if (AddDataNodeForAtomic(graph, clean_node, outputs_indexes.size()) != ge::SUCCESS) {
    GELOGE(ge::FAILED, "add data node for atomic clean node failed, outputs_indexes size = %zu",
           outputs_indexes.size());
    return nullptr;
  }
  std::vector<ge::NodePtr> in_out_nodes;
  auto in_nodes = clean_node->GetInNodes();
  auto out_nodes = clean_node->GetOutNodes();
  in_out_nodes.insert(in_out_nodes.cend(), in_nodes.begin(), in_nodes.end());
  in_out_nodes.insert(in_out_nodes.cend(), out_nodes.begin(), out_nodes.end());
  clean_node->GetOpDesc()->SetExtAttr("_in_out_nodes_holder", in_out_nodes);
  std::stringstream ss;
  for (const auto &clean_size : output_clean_sizes) {
    ss << clean_size << ",";
  }
  GELOGI("[AIC_INFO] atomic node %s", ss.str().c_str());
  return clean_node;
}

static bg::ValueHolderPtr LaunchAtomic(const ge::NodePtr &node, const LowerInput &lower_input,
                                 const LoweringGlobalData::NodeCompileResult *compile_result,
                                 const bg::AtomicLoweringArg &atomic_lowering_arg) {
  auto ctx = InitAtomicLaunchContext(node, compile_result, atomic_lowering_arg);
  if (ctx == nullptr) {
    return nullptr;
  }
  if (ctx->task_def == nullptr) {
    GELOGD("Atomic task definition not found, stopping atomic lowering process.");
    return nullptr;
  }
  uint32_t with_handle_flag = 0U;
  std::vector<bg::ValueHolderPtr> rt_arg;
  if (static_cast<ge::ModelTaskType>(ctx->task_def->type()) == ge::ModelTaskType::MODEL_TASK_ALL_KERNEL) {
    GELOGD("Node %s executed AllocRtArg with all kernels.", node->GetName().c_str());
    with_handle_flag = 1U;
    rt_arg = bg::AllocRtArg(node, ctx->task_def->kernel_with_handle(), bg::kMaxAtomicCleanTilingSize);
  } else {
    GELOGD("Node %s: AllocRtArg with single kernel.", node->GetNamePtr());
    rt_arg = bg::AllocRtArg(node, ctx->task_def->kernel(), bg::kMaxAtomicCleanTilingSize);
  }
  CHECK_HOLDERS_ALL_OK_RET(rt_arg, static_cast<size_t>(AllocLaunchArgOutputs::kNum), return nullptr);

  auto atomic_op_desc = ctx->atomic_node->GetOpDesc();
  auto origin_op_desc = node->GetOpDesc();
  std::string op_compile_info_json;
  if (!ge::AttrUtils::GetStr(origin_op_desc, optiling::ATOMIC_COMPILE_INFO_JSON, op_compile_info_json)) {
    GELOGE(ge::FAILED, "Op[%s] does not have attr[%s].", origin_op_desc->GetName().c_str(),
           optiling::ATOMIC_COMPILE_INFO_JSON.c_str());
    return nullptr;
  }
  if (!ge::AttrUtils::SetStr(atomic_op_desc, optiling::ATOMIC_COMPILE_INFO_JSON, op_compile_info_json)) {
    GELOGE(ge::FAILED, "Set attr[%s] for Op[%s] failed.", optiling::ATOMIC_COMPILE_INFO_JSON.c_str(),
           atomic_op_desc->GetName().c_str());
    return nullptr;
  }
  std::vector<bg::ValueHolderPtr> atomic_tiling_ret =
      bg::TilingForAtomic(ctx->atomic_node, atomic_lowering_arg.workspaces_size, ctx->output_clean_sizes,
                          rt_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)], *(lower_input.global_data));

  auto atomic_workspaces_addr = bg::AllocWorkspaceMem(kOnDeviceHbm, atomic_tiling_ret[TilingContext::kOutputWorkspace],
                                                      *(lower_input.global_data));
  gert::GertTensorData *tensor_data = nullptr;
  auto shapebuffer_addr = bg::ValueHolder::CreateConst(&tensor_data, sizeof(gert::GertTensorData *));

  auto atomic_bin_handle = ConstructAicoreBinHandle(node, true, *(lower_input.global_data));
  FE_ASSERT_NOTNULL(atomic_bin_handle);

  auto qos_pair = PrepareQosPair(ctx->task_def, ctx->atomic_node);
  if (qos_pair.second == nullptr) {
    return nullptr;
  }

  auto kernel_name_holder = CreateKernelNameHolder(node, kRawAtomicKernelNameKey);
  auto with_handle_flag_holder = CreateWithHandleFlagHolder(with_handle_flag);

  std::vector<int64_t> workspace_indexes;
  ge::AttrUtils::GetListInt(atomic_op_desc, "WorkspaceIndexes", workspace_indexes);
  DfxExeArg dfx_exe_arg = GetOpDfxExeArg(ctx->atomic_node);
  auto dfx_holder = bg::ValueHolder::CreateConst(&dfx_exe_arg, sizeof(dfx_exe_arg));
  auto launch_arg = bg::AtomicLaunchKernelV2(
      {
        lower_input.global_data->GetStream(),
        atomic_bin_handle,
        atomic_tiling_ret[TilingContext::kOutputBlockDim],
        atomic_tiling_ret[TilingContext::kOutputScheduleMode],
        atomic_tiling_ret[TilingContext::kOutputLocalMemorySize],
        atomic_workspaces_addr,
        shapebuffer_addr,
        qos_pair.first,
        qos_pair.second,
        {},
        {},
        ctx->atomic_node,
        lower_input.global_data,
        dfx_holder,
        atomic_tiling_ret[static_cast<size_t>(kernel::TilingExOutputIndex::kRtArg)],
        atomic_tiling_ret[TilingContext::kOutputTilingKey],
        kernel_name_holder,
        with_handle_flag_holder,
      },
      CreateVectorHolder(workspace_indexes), ctx->output_clean_addrs, atomic_lowering_arg.workspaces_addrs);
  auto free_holder = bg::FreeWorkspaceMem(kOnDeviceHbm, atomic_workspaces_addr);
  bg::ValueHolder::AddDependency(launch_arg, free_holder);

  return launch_arg;
}

static bg::ValueHolderPtr LaunchStaticAtomic(const ge::NodePtr &node, const LowerInput &lower_input,
                                       const LoweringGlobalData::NodeCompileResult *compile_result,
                                       const bg::AtomicLoweringArg &atomic_lowering_arg) {
  auto ctx = InitAtomicLaunchContext(node, compile_result, atomic_lowering_arg);
  if (ctx == nullptr) {
    return nullptr;
  }
  if (ctx->task_def == nullptr) {
    GELOGD("No static atomic task definition found, stopping atomic lowering.");
    return nullptr;
  }
  auto rt_arg = bg::AllocRtArg(node, ctx->task_def->kernel(), bg::kMaxAtomicCleanTilingSize);
  CHECK_HOLDERS_ALL_OK_RET(rt_arg, static_cast<size_t>(AllocLaunchArgOutputs::kNum), return nullptr);
  auto atomic_bin_handle = ConstructAicoreBinHandle(node, true, *(lower_input.global_data));
  auto block_dim_value = ctx->task_def->kernel().block_dim();
  auto block_dim = bg::ValueHolder::CreateConst(&block_dim_value, sizeof(block_dim_value));
  auto schedule_mode_value = ctx->task_def->kernel().schedule_mode();
  auto schedule_mode = bg::ValueHolder::CreateConst(&schedule_mode_value, sizeof(schedule_mode_value));
  uint32_t local_mem = 0;
  (void)ge::AttrUtils::GetInt(node->GetOpDesc(), kLocalMemorySize, local_mem);
  auto local_mem_size = bg::ValueHolder::CreateConst(&local_mem, sizeof(local_mem));
  ContinuousVector vec;
  vec.SetSize(0U);
  bg::ValueHolderPtr workspaces_addr = bg::ValueHolder::CreateConst(&vec, sizeof(vec));
  gert::GertTensorData *tensor_data = nullptr;
  auto shapebuffer_addr = bg::ValueHolder::CreateConst(&tensor_data, sizeof(gert::GertTensorData *));

  auto qos_pair = PrepareQosPair(ctx->task_def, ctx->atomic_node);
  if (qos_pair.second == nullptr) { return nullptr; }

  auto tiling_key_holder = CreateTilingKeyHolder();
  auto kernel_name_holder = CreateKernelNameHolder(node, kRawAtomicKernelNameKey);
  auto with_handle_flag_holder = CreateWithHandleFlagHolder(0U);

  std::vector<int64_t> workspace_indexes;
  ge::AttrUtils::GetListInt(ctx->atomic_node->GetOpDesc(), "WorkspaceIndexes", workspace_indexes);
  const bg::ValueHolderPtr &stream = lower_input.global_data->GetStream();
  DfxExeArg dfx_exe_arg = GetOpDfxExeArg(ctx->atomic_node);
  auto dfx_holder = bg::ValueHolder::CreateConst(&dfx_exe_arg, sizeof(dfx_exe_arg));
  auto launch_arg = bg::AtomicLaunchKernelV2({stream, atomic_bin_handle, block_dim, schedule_mode,
    local_mem_size, workspaces_addr, shapebuffer_addr, qos_pair.first, qos_pair.second, {}, {},
    ctx->atomic_node, lower_input.global_data, dfx_holder, rt_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)],
    tiling_key_holder, kernel_name_holder, with_handle_flag_holder},
    CreateVectorHolder(workspace_indexes), ctx->output_clean_addrs, atomic_lowering_arg.workspaces_addrs);
  return launch_arg;
}

bg::ValueHolderPtr LaunchAtomicByType(const ge::NodePtr &node, const LowerInput &lower_input,
                                       const LoweringGlobalData::NodeCompileResult *compile_result, 
                                       const bg::AtomicLoweringArg &atomic_lowering_arg) {
  bg::ValueHolderPtr atomic_launch_holder = nullptr;
  std::shared_ptr<optiling::utils::OpRunInfo> tiling_info = nullptr;
  tiling_info = node->GetOpDesc()->TryGetExtAttr(ge::ATTR_NAME_OP_RUN_INFO, tiling_info);
  if (!ge::AttrUtils::HasAttr(node->GetOpDesc(), optiling::ATOMIC_COMPILE_INFO_JSON) && tiling_info != nullptr) {
    GELOGD("Node %s has no ATOMIC_COMPILE_INFO_JSON.", node->GetName().c_str());
    atomic_launch_holder = LaunchStaticAtomic(node, lower_input, compile_result, atomic_lowering_arg);
  } else {
    GELOGD("Node %s has an ATOMIC_COMPILE_INFO_JSON.", node->GetNamePtr());
    atomic_launch_holder = LaunchAtomic(node, lower_input, compile_result, atomic_lowering_arg);
  }
  return atomic_launch_holder;
}

static std::optional<AicoreProcContext> PrepareAicoreProcContext(
    const ge::NodePtr &node,
    const LowerInput &lower_input,
    const domi::TaskDef *task_def,
    std::vector<bg::ValueHolderPtr> &output_shapes,
    ProcArgs &proc_arg) {
  AicoreProcContext ctx;
  ctx.workspaces_addr = bg::AllocAiCoreWorkspaceMem(node, kOnDeviceHbm,
      proc_arg.tiling_ret[TilingContext::kOutputWorkspace], *(lower_input.global_data));
  ctx.shapebuffer_addr = bg::AllocShapeBufferMem(kOnDeviceHbm, node, *(lower_input.global_data));
  ctx.output_sizes = bg::CalcTensorSize(node, output_shapes);
  ctx.output_addrs = bg::AllocOutputMemory(kOnDeviceHbm, node, ctx.output_sizes,
                                           lower_input.input_addrs, *(lower_input.global_data));

  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  ctx.atomic_launch_holder = LaunchAtomicByType(node, lower_input, compile_result,
                                                 {proc_arg.tiling_ret[TilingContext::kOutputWorkspace],
                                                   ctx.workspaces_addr, ctx.output_sizes, ctx.output_addrs});
  ctx.bin_handle = ConstructAicoreBinHandle(node, false, *(lower_input.global_data));

  ctx.qos_pair = PrepareQosPair(task_def, node);
  if (ctx.qos_pair.second == nullptr) {
    return std::nullopt;
  }
  return ctx;
}

bg::ValueHolderPtr ReportErrInRunning(const LowerInput &lower_input) {
  return bg::ValueHolder::CreateVoid<bg::ValueHolder>("ReportRollbackError",
                                                      ConvertDevMemValueHoldersToValueHolders(lower_input.input_addrs));
}

static std::vector<bg::DevMemValueHolderPtr> RollbackAiCpuProc(const ge::NodePtr &node, const LowerInput &lower_input,
                                                        const std::vector<bg::ValueHolderPtr> &output_shapes) {
  auto output_sizes = bg::CalcTensorSize(node, output_shapes);
  auto output_addrs = bg::AllocOutputMemory(kOnDeviceHbm, node, output_sizes,
                                            lower_input.input_addrs, *(lower_input.global_data));
  auto op_desc = node->GetOpDesc();
  // todo need common interface
  std::string kernel_lib_name;
  ge::AttrUtils::GetStr(op_desc, ge::kAICpuKernelLibName, kernel_lib_name);
  op_desc->SetOpKernelLibName(kernel_lib_name);
  GELOGD("Lowering node [%s] rollback to AiCpu, kernel lib is %s.", node->GetNamePtr(), kernel_lib_name.c_str());
  auto data = NodeConverterRegistry::GetInstance().FindRegisterData(ge::kEngineNameAiCpuTf);
  if (data == nullptr || data->converter == nullptr) {
    GELOGW("AiCpu lowering function is null.");
    (void)ReportErrInRunning(lower_input);
    return output_addrs;
  }
  LowerResult low_ret = data->converter(node, lower_input);
  std::vector<bg::DevMemValueHolderPtr> ret;
  if (!low_ret.result.IsSuccess()) {
    GELOGW("Lowering AiCpu node failed.");
    (void)ReportErrInRunning(lower_input);
    return output_addrs;
  } else {
    ret.reserve(output_shapes.size());
    ret.insert(ret.cend(), low_ret.out_addrs.cbegin(), low_ret.out_addrs.cend());
  }
  return ret;
}

static std::vector<bg::DevMemValueHolderPtr> AicoreProcWithHandle(const ge::NodePtr &node, const LowerInput &lower_input,
                                                            std::vector<bg::ValueHolderPtr> &output_shapes,
                                                            ProcArgs &proc_arg) {
  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  const domi::TaskDef *task_def = GetTaskDef(node, compile_result, TaskDefType::kAICore);
  if (task_def == nullptr) {
    return {};
  }
  auto ctx = PrepareAicoreProcContext(node, lower_input, task_def, output_shapes, proc_arg);
  if (!ctx.has_value()) {
    return {};
  }

  LaunchKernelParams params;
  params.tiling_key = proc_arg.tiling_ret[TilingContext::kOutputTilingKey];
  params.kernel_name_holder = CreateKernelNameHolder(node, kRawKernelNameKey);
  params.with_handle_flag_holder = CreateWithHandleFlagHolder(1U);

  return LaunchAicoreKernelAndProcess(node, lower_input, *ctx, output_shapes, proc_arg, params);
}

static std::vector<bg::DevMemValueHolderPtr> LoweringWithHandleProc(const ge::NodePtr &node, const LowerInput &lower_input,
                                                              ProcArgs &proc_arg,
                                                              std::vector<bg::ValueHolderPtr> &output_shapes) {
  // 2. tiling
  std::vector<bg::DevMemValueHolderPtr> output_addrs;
  auto platform_info_vec = bg::AppendCoreTypeToPlatform(node, lower_input.global_data);
  if (platform_info_vec.empty()) {
    GELOGE(ge::INTERNAL_ERROR, "platform_info_vec is empty! Node: %s.", node->GetName().c_str());
    return {};
  }
  auto platform_info = platform_info_vec[static_cast<size_t>(bg::AssemblePlatformInfoIndex::kPlatformInfo)];
  if (platform_info == nullptr) {
    GELOGE(ge::INTERNAL_ERROR, "platform_info is nullptr! Node: %s.", node->GetName().c_str());
    return {};
  }
  if (!NodeSupportRollback(node)) {
    proc_arg.tiling_ret = bg::Tiling(node, lower_input.input_shapes, output_shapes,
                                     {platform_info, *(lower_input.global_data),
                                      proc_arg.launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)]});
    CHECK_HOLDERS_ALL_OK_RET(proc_arg.tiling_ret, static_cast<size_t>(kernel::TilingExOutputIndex::kNum), return {});
    output_addrs = AicoreProcWithHandle(node, lower_input, output_shapes, proc_arg);
  } else {
    GELOGD("Lowering with rollback of aicpu.");
    proc_arg.tiling_ret = bg::FallibleTiling(node, lower_input.input_shapes, output_shapes,
                                             {platform_info, *(lower_input.global_data),
                                              proc_arg.launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)]});
    CHECK_HOLDERS_ALL_OK_RET(proc_arg.tiling_ret,
                             static_cast<size_t>(kernel::FallibleTilingExOutputIndex::kFallibleOutputNum), return {});
    output_addrs = bg::If<bg::DevMemValueHolder>(
        proc_arg.tiling_ret[static_cast<size_t>(kernel::FallibleTilingExOutputIndex::kTilingStatus)],
        [&node, &lower_input, &output_shapes, &proc_arg]() -> std::vector<bg::ValueHolderPtr> {
          auto result = AicoreProcWithHandle(node, lower_input, output_shapes, proc_arg);
          std::vector<bg::ValueHolderPtr> ret(result.begin(), result.end());
          return ret;
        },
        [&node, &lower_input, &output_shapes]() -> std::vector<bg::ValueHolderPtr> {
          auto result = RollbackAiCpuProc(node, lower_input, output_shapes);
          std::vector<bg::ValueHolderPtr> ret(result.begin(), result.end());
          return ret;
        },
        node->GetOpDesc()->GetStreamId());
    if (output_addrs.empty()) {
      GELOGE(ge::INTERNAL_ERROR, "Node %s lowering with rollback aicpu failed.", node->GetName().c_str());
      return output_addrs;
    }
    proc_arg.ordered_holders.clear();
    proc_arg.ordered_holders.emplace_back(output_addrs[0]);
  }
  FE_ASSERT_TRUE(!proc_arg.tiling_ret.empty());
  FE_ASSERT_NOTNULL(proc_arg.tiling_ret[TilingContext::kOutputTilingData]);

  return output_addrs;
}

template<typename LoweringProc>
static LowerResult LoweringAiCoreNodeCommon(
    const ge::NodePtr &node,
    const LowerInput &lower_input,
    ProcArgs &proc_arg,
    std::vector<bg::ValueHolderPtr> &output_shapes,
    const char* error_msg,
    LoweringProc lowering_proc) {
  if (!NeedCheckEmptyOutput(node)) {
    auto output_addrs = lowering_proc(node, lower_input, proc_arg, output_shapes);
    if (output_addrs.empty()) {
      if (node == nullptr || node->GetOpDesc() == nullptr ||
          node->GetOpDesc()->GetOutputsSize() != 0) {
        return {HyperStatus::ErrorStatus(error_msg), {}, {}, {}};
      }
    }
    if (proc_arg.atomic_launch != nullptr) {
      GELOGD("Node [%s] has added an atomic launch to the order holders.", node->GetName().c_str());
      proc_arg.ordered_holders.emplace_back(proc_arg.atomic_launch);
    }
    return {HyperStatus::Success(), proc_arg.ordered_holders, output_shapes, output_addrs};
  }

  bg::ValueHolderPtr cond = bg::ValueHolder::CreateSingleDataOutput("CheckOutputShapesEmpty", output_shapes);
  auto if_outputs = bg::If<bg::DevMemValueHolder>(
      cond,
      [&node, &output_shapes, &lower_input]() -> std::vector<bg::ValueHolderPtr> {
        auto output_sizes = bg::CalcTensorSize(node, output_shapes);
        auto result = bg::AllocOutputMemory(kOnDeviceHbm, node, output_sizes,
                                            lower_input.input_addrs, *(lower_input.global_data));
        std::vector<bg::ValueHolderPtr> ret(result.begin(), result.end());
        return ret;
      },
      [&]() -> std::vector<bg::ValueHolderPtr> {
        auto result = lowering_proc(node, lower_input, proc_arg, output_shapes);
        std::vector<bg::ValueHolderPtr> ret(result.begin(), result.end());
        return ret;
      },
      node->GetOpDesc()->GetStreamId());
  if (if_outputs.size() != output_shapes.size() || if_outputs.empty()) {
    return {HyperStatus::ErrorStatus(error_msg), {}, {}, {}};
  }
  return {HyperStatus::Success(), {if_outputs[0]}, output_shapes, if_outputs};
}

static LowerResult LoweringAiCoreNodeWithHandle(const ge::NodePtr &node, const LowerInput &lower_input) {
  GELOGD("Lowering node[%s] with handle begin.", node->GetNamePtr());
  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  // 0. alloc rt arg
  const domi::TaskDef *task_def = GetTaskDef(node, compile_result, TaskDefType::kAICore);
  if (task_def == nullptr) {
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Not find AI core task def")), {}, {}, {}};
  }
  ProcArgs proc_arg;
  proc_arg.launch_arg = bg::AllocRtArg(node, task_def->kernel_with_handle(), bg::kMaxTilingSize);
  CONVERTER_CHECK_HOLDERS_ALL_OK(proc_arg.launch_arg, static_cast<size_t>(AllocLaunchArgOutputs::kNum));
  auto output_shapes = InferAiCoreStorageShape(node, lower_input.input_shapes, *(lower_input.global_data));
  return LoweringAiCoreNodeCommon(node, lower_input, proc_arg, output_shapes,
                                   "Lowering with handle failed",
                                   LoweringWithHandleProc);
}

static std::vector<bg::DevMemValueHolderPtr> AicoreProcWithFlag(const ge::NodePtr &node, const LowerInput &lower_input,
                                                         std::vector<bg::ValueHolderPtr> &output_shapes,
                                                         ProcArgs &proc_arg) {
  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  const domi::TaskDef *task_def = GetTaskDef(node, compile_result, TaskDefType::kAICore);
  auto ctx = PrepareAicoreProcContext(node, lower_input, task_def, output_shapes, proc_arg);
  if (!ctx.has_value()) {
    return {};
  }

  LaunchKernelParams params;
  params.tiling_key = CreateTilingKeyHolder();
  params.kernel_name_holder = CreateKernelNameHolder(node, kRawKernelNameKey);
  params.with_handle_flag_holder = CreateWithHandleFlagHolder(0U);

  return LaunchAicoreKernelAndProcess(node, lower_input, *ctx, output_shapes, proc_arg, params);
}

static std::vector<bg::ValueHolderPtr> WithFlagTilingProc(const ge::NodePtr &node, const LowerInput &lower_input,
                                                          const std::vector<bg::ValueHolderPtr> &output_shapes,
                                                          const bg::ValueHolderPtr &platform_info,
                                                          const ProcArgs &proc_arg) {
  if (!node->GetOpDesc()->HasAttr(optiling::COMPILE_INFO_JSON)) {
    std::vector<bg::ValueHolderPtr> ret_v(static_cast<size_t>(kernel::TilingExOutputIndex::kNum), nullptr);
    const std::vector<int64_t> workspace_bytes = node->GetOpDesc()->GetWorkspaceBytes();
    auto work_space_holder = bg::CreateContVecHolder(workspace_bytes);
    FE_ASSERT_NOTNULL(work_space_holder);
    ret_v[TilingContext::kOutputWorkspace] = work_space_holder;
    int64_t block_dim = 1;
    (void)ge::AttrUtils::GetInt(node->GetOpDesc(), ge::TVM_ATTR_NAME_BLOCKDIM, block_dim);
    auto block_dim_holder = bg::ValueHolder::CreateConst(&block_dim, sizeof(block_dim));
    FE_ASSERT_NOTNULL(block_dim_holder);
    ret_v[TilingContext::kOutputBlockDim] = block_dim_holder;
    uint32_t schedule_mode = 0;
    (void)ge::AttrUtils::GetInt(node->GetOpDesc(), "_soft_sync_schedule_mode", schedule_mode);
    auto sche_holder = bg::ValueHolder::CreateConst(&schedule_mode, sizeof(schedule_mode));
    FE_ASSERT_NOTNULL(sche_holder);
    ret_v[TilingContext::kOutputScheduleMode] = sche_holder;
    ret_v[static_cast<size_t>(kernel::TilingExOutputIndex::kRtArg)] =
        proc_arg.launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)];
    uint32_t local_mem = 0;
    (void)ge::AttrUtils::GetInt(node->GetOpDesc(), kLocalMemorySize, local_mem);
    auto local_mem_size = bg::ValueHolder::CreateConst(&local_mem, sizeof(local_mem));
    FE_ASSERT_NOTNULL(local_mem_size);
    ret_v[TilingContext::kOutputLocalMemorySize] = local_mem_size;
    return ret_v;
  }
  return bg::Tiling(node, lower_input.input_shapes, output_shapes,
                    {platform_info, *(lower_input.global_data),
                     proc_arg.launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)]});
}

static std::vector<bg::DevMemValueHolderPtr> LoweringWithFlagProc(const ge::NodePtr &node, const LowerInput &lower_input,
                                                            ProcArgs &proc_arg,
                                                            std::vector<bg::ValueHolderPtr> &output_shapes) {
  // 2. tiling
  std::vector<bg::DevMemValueHolderPtr> output_addrs;
  auto platform_info_vec = bg::AppendCoreTypeToPlatform(node, lower_input.global_data);
  if (platform_info_vec.empty()) {
    GELOGE(ge::INTERNAL_ERROR, "platform_info_vec is empty! Node: %s.", node->GetName().c_str());
    return {};
  }
  auto platform_info = platform_info_vec[static_cast<size_t>(bg::AssemblePlatformInfoIndex::kPlatformInfo)];
  if (platform_info == nullptr) {
    GELOGE(ge::INTERNAL_ERROR, "platform_info is nullptr! Node: %s.", node->GetName().c_str());
    return {};
  }
  if (!NodeSupportRollback(node)) {
    proc_arg.tiling_ret = WithFlagTilingProc(node, lower_input, output_shapes, platform_info, proc_arg);
    output_addrs = AicoreProcWithFlag(node, lower_input, output_shapes, proc_arg);
  } else {
    GELOGD("Lowering with rollback of aicpu.");
    proc_arg.tiling_ret = bg::FallibleTiling(node, lower_input.input_shapes, output_shapes,
                                             {platform_info, *(lower_input.global_data),
                                              proc_arg.launch_arg[static_cast<size_t>(AllocLaunchArgOutputs::kRtArg)]});
    CHECK_HOLDERS_ALL_OK_RET(proc_arg.tiling_ret,
                             static_cast<size_t>(kernel::FallibleTilingExOutputIndex::kFallibleOutputNum), return {});
    output_addrs = bg::If<bg::DevMemValueHolder>(
        proc_arg.tiling_ret[static_cast<size_t>(kernel::FallibleTilingExOutputIndex::kTilingStatus)],
        [&node, &lower_input, &output_shapes, &proc_arg]() -> std::vector<bg::ValueHolderPtr> {
          auto result = AicoreProcWithFlag(node, lower_input, output_shapes, proc_arg);
          std::vector<bg::ValueHolderPtr> ret(result.begin(), result.end());
          return ret;
        },
        [&node, &lower_input, &output_shapes]() -> std::vector<bg::ValueHolderPtr> {
          auto result = RollbackAiCpuProc(node, lower_input, output_shapes);
          std::vector<bg::ValueHolderPtr> ret(result.begin(), result.end());
          return ret;
        },
        node->GetOpDesc()->GetStreamId());
    if (output_addrs.empty()) {
      GELOGE(ge::INTERNAL_ERROR, "Node %s lowering with rollback aicpu failed.", node->GetName().c_str());
      return output_addrs;
    }
    proc_arg.ordered_holders.clear();
    proc_arg.ordered_holders.emplace_back(output_addrs[0]);
  }
  FE_ASSERT_TRUE(!proc_arg.tiling_ret.empty());
  if(!node->GetOpDesc()->HasAttr(optiling::COMPILE_INFO_JSON)) {
    return output_addrs;
  }
  FE_ASSERT_NOTNULL(proc_arg.tiling_ret[TilingContext::kOutputTilingData]);

  return output_addrs;
}

static LowerResult LoweringAiCoreNodeWithFlag(const ge::NodePtr &node, const LowerInput &lower_input) {
  GELOGD("Lowering node[%s] with flag begin.", node->GetNamePtr());
  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  // 0. alloc rt arg
  const domi::TaskDef *task_def = GetTaskDef(node, compile_result, TaskDefType::kAICore);
  if (task_def == nullptr) {
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Not find AI core task def")), {}, {}, {}};
  }
  ProcArgs proc_arg;
  proc_arg.launch_arg = bg::AllocRtArg(node, task_def->kernel(), bg::kMaxTilingSize);
  CONVERTER_CHECK_HOLDERS_ALL_OK(proc_arg.launch_arg, static_cast<size_t>(AllocLaunchArgOutputs::kNum));
  auto output_shapes = InferAiCoreStorageShape(node, lower_input.input_shapes, *(lower_input.global_data));
  return LoweringAiCoreNodeCommon(node, lower_input, proc_arg, output_shapes,
                                   "Lowering with flag failed",
                                   LoweringWithFlagProc);
}

struct StaticAicoreContext {
  std::vector<bg::ValueHolderPtr> launch_arg;
  std::vector<bg::ValueHolderPtr> output_shapes;
  std::vector<bg::ValueHolderPtr> output_sizes;
  std::vector<bg::DevMemValueHolderPtr> output_addrs;
  bg::ValueHolderPtr bin_handle;
  bg::ValueHolderPtr shapebuffer_addr;
  bg::DevMemValueHolderPtr workspaces_addr;
  RunCfg run_cfg;
  bg::ValueHolderPtr atomic_launch_holder;
};

static std::optional<StaticAicoreContext> InitStaticAicoreContext(
    const ge::NodePtr &node, const LowerInput &lower_input,
    const domi::TaskDef *task_def, const LoweringGlobalData::NodeCompileResult *compile_result) {
  StaticAicoreContext ctx;
  ctx.launch_arg = bg::AllocRtArg(node, task_def->kernel(), bg::kMaxTilingSize);
  CHECK_HOLDERS_ALL_OK_RET(ctx.launch_arg, static_cast<size_t>(AllocLaunchArgOutputs::kNum), return std::nullopt);
  const auto &op_desc = node->GetOpDesc();
  ctx.output_shapes = CreateOutputShapes(op_desc);
  ctx.output_sizes = bg::CalcTensorSize(node, ctx.output_shapes);
  ctx.output_addrs = bg::AllocOutputMemory(kOnDeviceHbm, node, ctx.output_sizes,
                                           lower_input.input_addrs, *(lower_input.global_data));
  ctx.bin_handle = ConstructAicoreBinHandle(node, false, *(lower_input.global_data));
  gert::GertTensorData *tensor_data = nullptr;
  ctx.shapebuffer_addr = bg::ValueHolder::CreateConst(&tensor_data, sizeof(gert::GertTensorData *));
  ctx.run_cfg = CollectRunConfig(node, lower_input, task_def, ctx.output_shapes, ctx.launch_arg);
  if (ctx.run_cfg.block_dim == nullptr || ctx.run_cfg.schedule_mode == nullptr ||
      ctx.run_cfg.local_mem_size == nullptr || ctx.run_cfg.workspaces_size == nullptr) {
    return std::nullopt;
  }
  ctx.workspaces_addr = bg::AllocAiCoreWorkspaceMem(node, kOnDeviceHbm, ctx.run_cfg.workspaces_size,
                                                    *(lower_input.global_data));
  if (op_desc->HasAttr(optiling::ATOMIC_COMPILE_INFO_JSON)) {
    ctx.atomic_launch_holder = LaunchAtomic(node, lower_input, compile_result,
                                            {ctx.run_cfg.workspaces_size, ctx.workspaces_addr,
                                              ctx.output_sizes, ctx.output_addrs});
  } else {
    ctx.atomic_launch_holder = LaunchStaticAtomic(node, lower_input, compile_result,
                                                   {ctx.run_cfg.workspaces_size, ctx.workspaces_addr,
                                                    ctx.output_sizes, ctx.output_addrs});
  }
  return ctx;
}

static LowerResult LaunchStaticAicoreKernel(const ge::NodePtr &node, const LowerInput &lower_input,
                                             const StaticAicoreContext &ctx, const domi::TaskDef *task_def) {
  auto qos_pair = PrepareQosPair(task_def, node);
  if (qos_pair.second == nullptr) {
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Get qos info failed.")), {}, {}, {}};
  }
  DfxExeArg dfx_exe_arg = GetOpDfxExeArg(node);
  auto dfx_holder = bg::ValueHolder::CreateConst(&dfx_exe_arg, sizeof(dfx_exe_arg));
  auto launch_holder = bg::LaunchKernelV2(
      {lower_input.global_data->GetStream(), ctx.bin_handle, ctx.run_cfg.block_dim,
       ctx.run_cfg.schedule_mode, ctx.run_cfg.local_mem_size, ctx.workspaces_addr, ctx.shapebuffer_addr,
       qos_pair.first, qos_pair.second, lower_input.input_shapes, ctx.output_shapes, node,
       lower_input.global_data, dfx_holder, ctx.run_cfg.tiling_input_launch_arg,
       CreateTilingKeyHolder(), CreateKernelNameHolder(node, kRawKernelNameKey), CreateWithHandleFlagHolder(0U)},
      lower_input.input_addrs, ctx.output_addrs);
  std::vector<bg::ValueHolderPtr> order_holders = {launch_holder};
  auto free_holder = bg::FreeWorkspaceMem(kOnDeviceHbm, ctx.workspaces_addr);
  if (ctx.atomic_launch_holder != nullptr) {
    bg::ValueHolder::AddDependency(ctx.atomic_launch_holder, launch_holder);
    order_holders.emplace_back(ctx.atomic_launch_holder);
  }
  bg::ValueHolder::AddDependency(launch_holder, free_holder);
  return {HyperStatus::Success(), order_holders, ctx.output_shapes, ctx.output_addrs};
}

LowerResult LoweringStaticAicoreNode(const ge::NodePtr &node, const LowerInput &lower_input) {
  GELOGD("Lowering node[%s] with static aicore begin.", node->GetNamePtr());
  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  const domi::TaskDef *task_def = GetTaskDef(node, compile_result, TaskDefType::kAICore);
  if (task_def == nullptr) {
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Not find AI core task def")), {}, {}, {}};
  }
  auto ctx = InitStaticAicoreContext(node, lower_input, task_def, compile_result);
  if (!ctx.has_value()) {
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Init static context failed")), {}, {}, {}};
  }
  return LaunchStaticAicoreKernel(node, lower_input, *ctx, task_def);
}

LowerResult LoweringAiCoreNode(const ge::NodePtr &node, const LowerInput &lower_input) {
  auto ret = CheckLowerInput(lower_input);
  if (!ret.IsSuccess()) {
    return {ret, {}, {}, {}};
  }
  if (IsVirtualOp(node->GetOpDesc())) {
    GELOGD("Node [%s] is a virtual op; no need for lowering.", node->GetName().c_str());
    return {HyperStatus::Success(), {}, lower_input.input_shapes, lower_input.input_addrs};
  }
  auto compile_result = lower_input.global_data->FindCompiledResult(node);
  if (compile_result == nullptr) {
    REPORT_PREDEFINED_ERR_MSG("E22001", std::vector<const char *>({"opname", "optype"}),
                              std::vector<const char *>({node->GetName().c_str(), ge::NodeUtils::GetNodeType(node).c_str()}));
    GELOGE(ge::PARAM_INVALID, "Can not find compile result for node %s type %s", node->GetName().c_str(),
           ge::NodeUtils::GetNodeType(node).c_str());
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Can not find compile result")), {}, {}, {}};
  }
  if (compile_result->task_defs.empty()) {
    GELOGE(ge::PARAM_INVALID, "Unexpected task defs count %zu", compile_result->task_defs.size());
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Unexpected task defs count")), {}, {}, {}};
  }
  const domi::TaskDef *task_def = GetTaskDef(node, compile_result, TaskDefType::kAICore);
  if (task_def == nullptr) {
    return {HyperStatus::ErrorStatus(static_cast<const char *>("Not find AI core taskdef.")), {}, {}, {}};
  }
  const auto &op_desc = node->GetOpDesc();
  if (op_desc == nullptr) {
    return {ret, {}, {}, {}};
  }

  std::shared_ptr<optiling::utils::OpRunInfo> tiling_info = nullptr;
  tiling_info = op_desc->TryGetExtAttr(ge::ATTR_NAME_OP_RUN_INFO, tiling_info);
  LowerResult lower_ret;
  // todo max_size用在申请args内存
  if (static_cast<ge::ModelTaskType>(task_def->type()) == ge::ModelTaskType::MODEL_TASK_ALL_KERNEL ||
      static_cast<ge::ModelTaskType>(task_def->type()) == ge::ModelTaskType::MODEL_TASK_VECTOR_ALL_KERNEL) {
    lower_ret = LoweringAiCoreNodeWithHandle(node, lower_input);
  } else {
    if (bg::IsUnkownShape(op_desc) || tiling_info != nullptr) {
      lower_ret = LoweringAiCoreNodeWithFlag(node, lower_input);
    } else {
      lower_ret = LoweringStaticAicoreNode(node, lower_input);
    }
  }
  if (!lower_ret.result.IsSuccess()) {
    REPORT_FE_ERROR("Node[%s] lowering failed.", node->GetName().c_str());
  }
  return lower_ret;
}
REGISTER_NODE_CONVERTER_PLACEMENT(ge::kEngineNameAiCore.c_str(), kOnDeviceHbm, LoweringAiCoreNode);
REGISTER_NODE_CONVERTER_PLACEMENT(ge::kEngineNameVectorCore.c_str(), kOnDeviceHbm, LoweringAiCoreNode);
}  // namespace gert
