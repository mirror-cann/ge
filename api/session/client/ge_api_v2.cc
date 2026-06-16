/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ge/ge_api_v2.h"
#include <atomic>
#include <iostream>
#include <malloc.h>
#include "ge_is_initialize.h"
#include "common/compile_profiling/ge_call_wrapper.h"
#include "common/plugin/datatype_util.h"
#include "common/plugin/plugin_manager.h"
#include "common/plugin/tbe_plugin_manager.h"
#include "common/profiling/profiling_init.h"
#include "common/profiling/profiling_properties.h"
#include "base/err_msg.h"
#include "rt_error_codes.h"
#include "ge/ge_api_types.h"
#include "register/custom_pass_helper.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/debug/log.h"
#include "framework/common/ge_types.h"
#include "framework/executor/ge_executor.h"
#include "framework/memory/memory_api.h"
#include "framework/common/helper/model_helper.h"
#include "graph/detail/model_serialize_imp.h"
#include "graph/ge_context.h"
#include "graph/manager/util/rt_context_util.h"
#include "graph/manager/graph_external_weight_manager.h"
#include "graph/model_serialize.h"
#include "graph/opsproto_manager.h"
#include "base/registry/opp_package_utils.h"
#include "register/op_lib_register_impl.h"
#include "graph/utils/type_utils.h"
#include "api/gelib/gelib.h"
#include "api/aclgrph/option_utils.h"
#include "proto/ge_api.pb.h"
#include "register/op_registry.h"
#include "runtime/v2/core/debug/kernel_tracing.h"
#include "session_v2/ge_session_manager.h"
#include "session_v2/ge_session_impl.h"
#include "plog.h"
#include "common/checker.h"
#include "framework/runtime/subscriber/global_profiler.h"
#include "common/option_supportion_checker.h"
#include "exec_runtime/execution_runtime_utils.h"
#include "dflow/base/exec_runtime/execution_runtime.h"
#include "base/err_msg.h"
#include "common/memory/tensor_trans_utils.h"
#include "graph/operator_factory_impl.h"

namespace {
constexpr int32_t kMaxStrLen = 128;
constexpr uint32_t kExternalErrorCodeMaxValue = 9999999U;  // user define error code max value
constexpr size_t kGesizefloat = sizeof(float);
constexpr size_t kGesizehalffloat = sizeof(float) / 2U;
constexpr size_t kGesizefloat8 = sizeof(int8_t);
constexpr size_t kGesizeint8 = sizeof(int8_t);
constexpr size_t kGesizeint16 = sizeof(int16_t);
constexpr size_t kGesizeint32 = sizeof(int32_t);
constexpr size_t kGesizeint64 = sizeof(int64_t);
constexpr size_t kGesizeuint8 = sizeof(uint8_t);
constexpr size_t kGesizebool = sizeof(bool);
constexpr size_t kGesizedouble = sizeof(double);
constexpr size_t kGesizeuint64 = sizeof(uint64_t);
constexpr size_t kGesizeuint16 = sizeof(uint16_t);
constexpr size_t kGesizeuint32 = sizeof(uint32_t);

std::map<ge::DataType, size_t> CONST_OPDATA_TYPE_SIZE_MAP = {
    {ge::DT_FLOAT, kGesizefloat},        {ge::DT_FLOAT16, kGesizehalffloat},  {ge::DT_INT8, kGesizeint8},
    {ge::DT_INT16, kGesizeint16},        {ge::DT_INT32, kGesizeint32},        {ge::DT_INT64, kGesizeint64},
    {ge::DT_UINT8, kGesizeuint8},        {ge::DT_UINT16, kGesizeuint16},      {ge::DT_UINT32, kGesizeuint32},
    {ge::DT_UINT64, kGesizeuint64},      {ge::DT_DOUBLE, kGesizedouble},      {ge::DT_BOOL, kGesizebool},
    {ge::DT_HIFLOAT8, kGesizefloat8},    {ge::DT_FLOAT8_E5M2, kGesizefloat8}, {ge::DT_FLOAT8_E4M3FN, kGesizefloat8},
    {ge::DT_FLOAT8_E8M0, kGesizefloat8}, {ge::DT_FLOAT6_E3M2, kGesizefloat8}, {ge::DT_FLOAT6_E2M3, kGesizefloat8},
    {ge::DT_FLOAT4_E2M1, kGesizefloat8}, {ge::DT_FLOAT4_E1M2, kGesizefloat8},
};

ge::Status InitProfiling(const std::map<std::string, std::string> &options) {
  GELOGD("InitProfiling.");
  const ge::Status ret = ge::ProfilingInit::Instance().Init(options);
  if (ret != ge::SUCCESS) {
    GELOGE(ge::FAILED, "[Init][Profiling] Profiling init failed.");
    return ge::FAILED;
  }
  return ge::SUCCESS;
}

void SetOptionNameMap(const std::map<std::string, std::string> &options) {
  const auto iter = options.find(ge::OPTION_NAME_MAP);
  if (iter != options.end()) {
    (void)ge::GetContext().SetOptionNameMap(iter->second);
  }
}

void ShutDownProfiling() {
  GELOGD("Profiling shut down.");
  ge::ProfilingInit::Instance().ShutDownProfiling();
}
}  // namespace

static std::atomic_bool g_ge_initialized{false};
static std::mutex g_ge_release_mutex;  // GEFinalize and ~GeSession use
static std::shared_ptr<ge::GeSessionManager> g_ge_session_manager;

ge::GeSessionManager *GetGeSessionManager() {
  return g_ge_session_manager.get();
}

namespace ge {
namespace {
void ConstructSession(const std::map<std::string, std::string> &options, SessionId &session_id) {
  GELOGT(TRACE_INIT, "GeSession Constructor start");
  // check init status
  session_id = 0U;
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "Construct session failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Construct session failed because lack GEInitialize call before.");
    return;
  }
  // call Initialize
  if (GEAPICheckSupportedSessionOptions(options) != SUCCESS) {
    GELOGW("[Check][Param] Check supported options failed.");
  }
  if (CheckAllowParallelCompile(options) != SUCCESS) {
    return;
  }
  uint64_t tmp_session_id = 0UL;
  const Status ret = g_ge_session_manager->CreateSession(options, tmp_session_id);
  // failed guarder, should call GE_DISMISS_GUARD if success
  GE_DISMISSABLE_GUARD(create_failed, ([tmp_session_id]() { g_ge_session_manager->DestroySession(tmp_session_id); }));
  if (ret != SUCCESS) {
    GELOGE(ret, "Construct session failed, error code:%u.", ret);
    REPORT_INNER_ERR_MSG("E19999", "Construct session failed, error code:%u.", ret);
    return;
  }
  session_id = tmp_session_id;
  GE_DISMISS_GUARD(create_failed);
  GELOGT(TRACE_STOP, "GeSession construct finished, session id is %lu", session_id);
}
}  // namespace

static Status CheckOptionsValid(const std::map<std::string, std::string> &options) {
  const auto &iter = options.find("ge.autoTuneMode");
  if ((iter != options.end()) && (!iter->second.empty())) {
    REPORT_INNER_ERR_MSG("E19999",
                         "Options unsupport, [ge.autoTuneMode] has been discarded. Please use AOE tool for tuning.");
    GELOGE(FAILED,
           "[Check][Param]Options unsupport, [ge.autoTuneMode] has been discarded. Please use AOE tool for tuning.");
    return FAILED;
  }

  // check job_id is valid
  const auto &job_id_iter = options.find(OPTION_EXEC_JOB_ID);
  if (job_id_iter != options.end()) {
    if (job_id_iter->second.length() > static_cast<size_t>(kMaxStrLen)) {
      GELOGE(PARAM_INVALID,
             "[Check][JobId]Failed, the job_id [%s] std::string length: %zu > max std::string length: %d",
             job_id_iter->second.c_str(), job_id_iter->second.length(), kMaxStrLen);
      REPORT_PREDEFINED_ERR_MSG(
          "E10051", std::vector<const char *>({"id", "length"}),
          std::vector<const char *>({job_id_iter->second.c_str(), std::to_string(kMaxStrLen).c_str()}));
      return FAILED;
    }
  }

  GE_ASSERT_SUCCESS(CheckPrecisionModeParamValid(options));
  GE_ASSERT_SUCCESS(CheckModifyMixlistParamValid(options));
  GE_ASSERT_SUCCESS(CheckOptionValidValues(options, OPTION_FEATURE_BASE_REFRESHABLE, kFeatureMapRefreshOptions));
  GE_ASSERT_SUCCESS(CheckOptionValidValues(options, OPTION_CONST_LIFECYCLE, kConstLifecycleOptions));
  GE_ASSERT_SUCCESS(CheckOptionValidValues(options, ENABLE_SINGLE_STREAM, kBoolOptions));
  GE_ASSERT_SUCCESS(CheckOptionValidThreshold(options, OPTION_HOST_SCHEDULING_MAX_THRESHOLD));
  GE_ASSERT_SUCCESS(CheckOptionValidValues(options, TILING_SCHEDULE_OPTIMIZE, kStateOptions));
  GE_ASSERT_GRAPH_SUCCESS(CheckOptimizationOptionValid(options));
  GE_ASSERT_SUCCESS(CheckAllowParallelCompile(options));
  return SUCCESS;
}

Status InitializeExecutionRuntime(const std::map<std::string, std::string> &options) {
  if (ExecutionRuntime::GetInstance() == nullptr) {
    if (ExecutionRuntimeUtils::IsHeterogeneous()) {
      GE_CHK_STATUS_RET_NOLOG(ExecutionRuntime::InitHeterogeneousRuntime(options));
    }
    GELOGI("Execution runtime initialize success.");
  }
  return SUCCESS;
}

// Initialize GE, prepare for execution, call GELib::Initialize
static Status GEInitializeImpl(const std::map<std::string, std::string> &options) {
  GE_TIMESTAMP_START(GEInitializeAll);
  GELOGT(TRACE_INIT, "GEInitialize start");
  for (const auto &item : options) {
    GELOGI("GE option: %s, value: %s.", item.first.c_str(), item.second.c_str());
  }

  // 0.check init status
  if (g_ge_initialized) {
    GELOGW("GEInitialize is called more than once");
    return SUCCESS;
  }
  GE_TIMESTAMP_START(GEAPICheckSupportedGlobalOptions);
  if (GEAPICheckSupportedGlobalOptions(options) != SUCCESS) {
    GELOGW("[Check][Param] Check supported options failed.");
  }
  GE_TIMESTAMP_EVENT_END(GEAPICheckSupportedGlobalOptions, "GEInitialize::GEAPICheckSupportedGlobalOptions");

  // 备份并清空注册信息map
  OperatorFactoryImpl::BackupAndClearRegInfoOnce();

  // 2. init error ErrorManager
  Status ret = static_cast<uint32_t>(error_message::ErrMgrInit(error_message::ErrorMessageMode::INTERNAL_MODE));
  GE_ASSERT_SUCCESS(ret);

  SetOptionNameMap(options);

  // 3. check options is valid
  GE_TIMESTAMP_START(CheckOptionsValid);
  GE_ASSERT_SUCCESS(CheckOptionsValid(options));
  GE_TIMESTAMP_EVENT_END(CheckOptionsValid, "GEInitialize::CheckOptionsValid");

  // 4. init OpsProto lib plugin
  GE_ASSERT_GRAPH_SUCCESS(OpLibRegistry::GetInstance().PreProcessForCustomOp());

  // 加载顺序遵循3.0目录结构，按op_graph->op_impl->op_proto->framework顺序加载，详细规则见PluginManager::GetOpsProtoPath注释
  gert::OppPackageUtils::LoadAllOppPackage();
  std::string opsproto_path;
  ret = PluginManager::GetOpsProtoPath(opsproto_path);
  if (ret != SUCCESS) {
    GELOGW("Failed to get ops proto path!");
  }
  OpsProtoManager *manager = OpsProtoManager::Instance();
  std::map<std::string, std::string> option_tmp;
  (void)option_tmp.emplace(std::pair<std::string, std::string>(string("ge.opsProtoLibPath"), opsproto_path));
  GE_TIMESTAMP_START(GEInitialize);
  const bool is_proto_init = manager->Initialize(option_tmp);
  GE_TIMESTAMP_EVENT_END(GEInitialize, "GEInitialize::ManagerInitialize");
  if (!is_proto_init) {
    GELOGE(GE_CLI_INIT_FAILED, "[Init][OpsProtoPath]Loading OpsProto lib plugin failed, OpsProtoPath:%s invalid.",
           opsproto_path.c_str());
    REPORT_INNER_ERR_MSG("E19999", "Loading OpsProto lib plugin failed, OpsProtoPath:%s invalid",
                         opsproto_path.c_str());
    return FAILED;
  }
  GE_ASSERT_SUCCESS(CustomPassHelper::Instance().Load());

  ge::GetContext().Init();

  // 5. init profiling
  GE_TIMESTAMP_START(InitProfiling);
  ret = InitProfiling(options);
  if (ret != SUCCESS) {
    GELOGE(GE_CLI_INIT_FAILED, "[Init][Profiling]Failed, error code = %u", ret);
    return FAILED;
  }
  GE_TIMESTAMP_EVENT_END(InitProfiling, "GEInitialize::InitProfiling");

  // 6. init tbe PluginManager
  GE_TIMESTAMP_START(InitPreparation);
  TBEPluginManager::Instance().InitPreparation(options);
  GE_TIMESTAMP_EVENT_END(InitPreparation, "GEInitialize::InitPreparation");

  // 7. gelib Initialize
  GE_TIMESTAMP_START(GELibInitialize);
  ret = ge::GELib::Initialize(options);
  GE_TIMESTAMP_EVENT_END(GELibInitialize, "GEInitialize::GELibInitialize");
  if (ret != SUCCESS) {
    GELOGE(GE_CLI_INIT_FAILED, "[Init][GELib]Failed, error code = %u", ret);
    return FAILED;
  }

  // 8. init session manager
  GELOGI("GeSessionManager initial.");
  GE_TIMESTAMP_START(GeSessionManagerInitialize);
  g_ge_session_manager = MakeShared<ge::GeSessionManager>();
  if (g_ge_session_manager == nullptr) {
    GELOGE(GE_CLI_INIT_FAILED, "[Init][Create]GeSessionManager failed");
    return FAILED;
  }
  ret = g_ge_session_manager->Initialize();
  GE_TIMESTAMP_EVENT_END(GeSessionManagerInitialize, "InnerInitialize::GeSessionManagerInitialize");
  if (ret != SUCCESS) {
    GELOGE(ret, "[Init][GeSessionManager] GE session manager initial failed.");
    REPORT_INNER_ERR_MSG("E19999", "GeSessionManager initialize failed.");
    return ret;
  }

  // 9. init GeExecutor
  GELOGI("GeExecutor initial.");
  GE_TIMESTAMP_START(GeExecutorInitialize);
  ret = GeExecutor::Initialize(options);
  if (ret != SUCCESS) {
    GELOGE(ret, "[Init][GeExecutor] GeExecutor initialize failed.");
    REPORT_INNER_ERR_MSG("E19999", "GeExecutor initialize failed.");
    return ret;
  }
  GE_TIMESTAMP_EVENT_END(GeExecutorInitialize, "GeExecutor::Initialize");
  GE_TIMESTAMP_START(InitializeExecutionRuntime);
  GE_CHK_STATUS_RET(InitializeExecutionRuntime(options), "Failed to init execution runtime");
  GE_TIMESTAMP_EVENT_END(InitializeExecutionRuntime, "InitializeExecutionRuntime");
  //将备份的注册信息低优先级merge到当前map
  OperatorFactoryImpl::MergeBackupCreatorsOnce();
  // 7. init ge init status, first time calling initialize
  if (!g_ge_initialized) {
    g_ge_initialized = true;
  }

  GELOGT(TRACE_STOP, "GEInitialize finished");
  GE_TIMESTAMP_EVENT_END(GEInitializeAll, "GEInitialize::All");
  return ret;
}

Status GEInitializeV2(const std::map<AscendString, AscendString> &options) {
  std::map<std::string, std::string> str_options;
  for (const auto &option_item : options) {
    if (option_item.first.GetLength() == 0) {
      GELOGE(FAILED, "[Check][Param] GEInitialize failed, option key is empty.");
      REPORT_INNER_ERR_MSG("E19999", "Check parameter's options invalid, option key is empty.");
      return FAILED;
    }
    const std::string &key = std::string(option_item.first.GetString(), option_item.first.GetLength());
    const std::string &val = std::string(option_item.second.GetString(), option_item.second.GetLength());
    str_options[key] = val;
  }

  if (static_cast<uint32_t>(DlogReportInitialize()) != SUCCESS) {
    GELOGW("Dlog report device log initialize failed.");
  }

  const auto ret = GEInitializeImpl(str_options);
  if (ret != SUCCESS) {
    GELOGE(ret, "[GEInit] GEInitialize failed, error code:%u.", ret);
    REPORT_INNER_ERR_MSG("E19999", "GEInitialize failed.");
  }
  return ret;
}

bool IsGEInitialize() {
  return g_ge_initialized;
}

// GE finalize, releasing all resources
Status GEFinalizeV2() {
  GRAPH_PROFILING_REG(gert::GeProfInfoType::kGEFinalize);
  // check init status
  if (!g_ge_initialized) {
    GELOGW("[FINAL]GEFinalize is called before GEInitialize");
    return SUCCESS;
  }
  std::lock_guard<std::mutex> lock(g_ge_release_mutex);
  GELOGT(TRACE_INIT, "GEFinalize start");

  GELOGI("GeSessionManager finalization.");
  if (g_ge_session_manager != nullptr) {
    (void)g_ge_session_manager->Finalize();  // always success.
  }
  ShutDownProfiling();

  (void)CustomPassHelper::Instance().Unload();
  // call Finalize
  (void)GeExecutor::FinalizeEx();
  ExecutionRuntime::FinalizeExecutionRuntime();
  Status ret = SUCCESS;
  Status middle_ret;
  std::shared_ptr<GELib> ge_lib = GELib::GetInstance();
  if (ge_lib != nullptr) {
    middle_ret = ge_lib->Finalize();
    GELOGI("GEFinalize finalize gelib ret=%u", middle_ret);
    if (middle_ret != SUCCESS) {
      ret = middle_ret;
    }
  }

  middle_ret = TBEPluginManager::Instance().Finalize();
  if (middle_ret != SUCCESS) {
    ret = middle_ret;
  }

  if (g_ge_initialized && (ret == SUCCESS)) {
    // Unified destruct rt_context
    RtContextUtil::GetInstance().DestroyAllRtContexts();
    g_ge_initialized = false;
  }

  // to avoid memory fragment, use malloc_trim to back free stack to system
  (void)malloc_trim(0);

  if (static_cast<uint32_t>(DlogReportFinalize()) != SUCCESS) {
    GELOGW("Dlog report device log finalize failed.");
  }

  GELOGT(TRACE_STOP, "GEFinalize finished");
  return ret;
}

ge::AscendString GEGetErrorMsgV3() {
  return ge::AscendString(error_message::GetErrMgrErrorMessage().get());
}

ge::AscendString GEGetWarningMsgV3() {
  return ge::AscendString(error_message::GetErrMgrWarningMessage().get());
}

GeSession::GeSession(const std::map<AscendString, AscendString> &options) : impl_(std::make_shared<GeSession::Impl>()) {
  std::map<std::string, std::string> str_options;
  for (auto &option_item : options) {
    if (option_item.first.GetLength() == 0) {
      GELOGE(FAILED, "Construct session failed, option key is empty.");
      REPORT_INNER_ERR_MSG("E19999", "Construct session failed, option key is empty.");
      return;
    }
    const std::string &key = option_item.first.GetString();
    const std::string &val = option_item.second.GetString();
    str_options[key] = val;
  }
  if (impl_ == nullptr) {
    GELOGE(FAILED, "GeSession failed, impl_ is null.");
    REPORT_INNER_ERR_MSG("E19999", "GeSession failed, impl_ is null.");
    return;
  }
  ge::SessionId session_id;
  ConstructSession(str_options, session_id);
  impl_->SetSessionId(session_id);
}

// session destructor
GeSession::~GeSession() {
  GELOGT(TRACE_INIT, "Start to destroy session.");
  // 0.check init status
  if (!g_ge_initialized) {
    GELOGW("GE is not yet initialized or is finalized.");
    return;
  }
  ExternalWeightManagerPool::Instance().RemoveManager(GetSessionId());
  Status ret = FAILED;
  std::lock_guard<std::mutex> lock(g_ge_release_mutex);
  try {
    const uint64_t session_id = GetSessionId();
    // call DestroySession
    GELOGT(TRACE_RUNNING, "GeSession id is %lu", session_id);
    ret = g_ge_session_manager->DestroySession(session_id);
  } catch (std::exception &e) {
    (void)e;
    GELOGE(GE_CLI_SESS_DESTROY_FAILED, "[Destructor][GeSession]Failed: an exception occurred");
    REPORT_INNER_ERR_MSG("E19999", "Failed to destroy session: an exception occurred");
  }

  // check return status, return, update session id if success
  if (ret != SUCCESS) {
    GELOGE(ret, "[Destructor][GeSession]Failed, error code:%u.", ret);
    REPORT_INNER_ERR_MSG("E19999", "Destroy session failed, error code:%u.", ret);
  }

  GELOGT(TRACE_STOP, "GeSession has been successfully destroyed");
}
Status GeSession::AddGraph(uint32_t graph_id, const Graph &graph) {
  return AddGraph(graph_id, graph, {});
}

// Add Graph
Status GeSession::AddGraph(uint32_t graph_id, const Graph &graph, const std::map<AscendString, AscendString> &options) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Add graph failed, session_id:%lu.", GetSessionId());

  AscendString graph_name;
  GE_ASSERT_SUCCESS(graph.GetName(graph_name), "Add graph failed, get graph name failed.");
  GELOGT(TRACE_INIT, "Start to add graph in GeSession. graph_id: %u, graph_name: %s, session_id: %lu.", graph_id,
         graph_name.GetString(), GetSessionId());

  std::map<std::string, std::string> str_options;
  for (auto &option_item : options) {
    if (option_item.first.GetLength() == 0) {
      GELOGE(FAILED, "Add graph failed, option key is empty.");
      REPORT_INNER_ERR_MSG("E19999", "Add graph failed, option key is empty.");
      return FAILED;
    }

    const std::string &key = option_item.first.GetString();
    const std::string &val = option_item.second.GetString();
    str_options[key] = val;
  }

  if (GEAPICheckSupportedGraphOptions(str_options) != SUCCESS) {
    GELOGW("[Check][Param] Check supported options failed.");
  }
  GELOGD("Adding graph to session");
  Status ret = inner_session->AddGraph(graph_id, graph, str_options);
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, FAILED, "Add graph failed, error code:%u, session_id:%lu, graph_id:%u.", ret,
                         GetSessionId(), graph_id);

  GELOGD("AddGraph finished in GeSession, graph_id: %u", graph_id);
  return SUCCESS;
}

Status GeSession::AddGraphClone(uint32_t graph_id, const Graph &graph) {
  return AddGraphClone(graph_id, graph, {});
}

// Add Graph With Copy
Status GeSession::AddGraphClone(uint32_t graph_id, const Graph &graph,
                                const std::map<AscendString, AscendString> &options) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Add graph failed, session_id:%lu.", GetSessionId());

  AscendString graph_name;
  GE_ASSERT_SUCCESS(graph.GetName(graph_name), "Add graph failed, get graph name failed.");
  GELOGT(TRACE_INIT, "Start to add graph in GeSession. graph_id: %u, graph_name: %s, session_id: %lu.", graph_id,
         graph_name.GetString(), GetSessionId());

  std::map<std::string, std::string> str_options;
  for (auto it = options.begin(); it != options.end(); ++it) {
    (void)str_options.emplace(it->first.GetString(), it->second.GetString());
  }

  if (GEAPICheckSupportedGraphOptions(str_options) != SUCCESS) {
    GELOGW("[Check][Param] Check supported options failed.");
  }

  GELOGD("Adding graph to session");
  const Status ret = inner_session->AddGraphWithCopy(graph_id, graph, str_options);
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, FAILED, "Add graph failed, error code:%u, session_id:%lu, graph_id:%u.", ret,
                         GetSessionId(), graph_id);

  GELOGD("AddGraph finished in GeSession.");
  return ret;
}

// Remove Graph
Status GeSession::RemoveGraph(uint32_t graph_id) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Remove graph failed, session_id:%lu.", GetSessionId());

  GRAPH_PROFILING_REG(gert::GeProfInfoType::kRemoveGraph);
  GELOGT(TRACE_INIT, "GeSession RemoveGraph start, graph_id: %u", graph_id);

  // call RemoveGraph
  Status ret = inner_session->RemoveGraph(graph_id);
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, FAILED, "Remove graph failed, error code:%u, session_id:%lu, graph_id:%u.",
                         ret, GetSessionId(), graph_id);

  GELOGT(TRACE_STOP, "GeSession RemoveGraph finished, graph_id: %u", graph_id);
  return ret;
}

static void PrintOutputResult(std::vector<gert::Tensor> &outputs) {
  if (outputs.empty() || (outputs[0].GetAddr() == nullptr)) {
    GELOGW("outputs is empty or data is nullptr.");
    return;
  }

  const DataType data_type = outputs[0].GetDataType();
  if (CONST_OPDATA_TYPE_SIZE_MAP.find(data_type) == CONST_OPDATA_TYPE_SIZE_MAP.end()) {
    GELOGI("DataType %s has not defined size", TypeUtils::DataTypeToSerialString(data_type).c_str());
    return;
  }
  const auto addr = outputs[0].GetAddr();
  // take first 10 at most
  for (size_t i = 0UL; (i < 10UL) && (i < (outputs[0].GetSize() / CONST_OPDATA_TYPE_SIZE_MAP[data_type])); ++i) {
    switch (data_type) {
      case DT_BOOL:
      case DT_INT8:
      case DT_UINT8:
      case DT_HIFLOAT8:
      case DT_FLOAT8_E5M2:
      case DT_FLOAT8_E4M3FN:
      case DT_FLOAT8_E8M0:
      case DT_FLOAT6_E3M2:
      case DT_FLOAT6_E2M3:
      case DT_FLOAT4_E2M1:
      case DT_FLOAT4_E1M2:
        GELOGI("output data[%zu]=%d", i, *(reinterpret_cast<int8_t *>(addr) + i));
        break;
      case DT_INT16:
      case DT_UINT16:
        GELOGI("output data[%zu]=%d", i, *(reinterpret_cast<int16_t *>(addr) + i));
        break;
      case DT_INT32:
      case DT_UINT32:
        GELOGI("output data[%zu]=%d", i, *(reinterpret_cast<int32_t *>(addr) + i));
        break;
      case DT_INT64:
      case DT_UINT64:
        GELOGI("output data[%zu]=%ld", i, *(reinterpret_cast<int64_t *>(addr) + i));
        break;
      case DT_FLOAT:
        GELOGI("output data[%zu]=%f", i, *(reinterpret_cast<float *>(addr) + i));
        break;
      case DT_DOUBLE:
        GELOGI("output data[%zu]=%lf", i, *(reinterpret_cast<double *>(addr) + i));
        break;
      default:
        GELOGI("Output datatype %s is not supported.", TypeUtils::DataTypeToSerialString(data_type).c_str());
        return;
    }
  }
}

// Run Graph
Status GeSession::RunGraph(uint32_t graph_id, const std::vector<gert::Tensor> &inputs,
                           std::vector<gert::Tensor> &outputs) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Run graph failed, session_id:%lu.", GetSessionId());

  GELOGI("GeSession RunGraph start, session_id: %lu, graph_id: %u, input size %zu, output size %zu", GetSessionId(),
         graph_id, inputs.size(), outputs.size());
  outputs.clear();
  Status ret = inner_session->RunGraph(graph_id, inputs, outputs);
  // check return status
  const bool need_convert_error_code = (ret == RT_ERROR_TO_GE_STATUS(ACL_ERROR_RT_QUEUE_EMPTY));
  ret = need_convert_error_code ? ACL_ERROR_GE_MODEL_EXECUTE_TIMEOUT : ret;
  const auto status = ret > kExternalErrorCodeMaxValue ? FAILED : ret;
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, status, "Run graph failed, error code:%u, session_id:%lu, graph_id:%u.", ret,
                         GetSessionId(), graph_id);
  // print output
  if (outputs.size() > 0U) {
    PrintOutputResult(outputs);
  }

  // return
  GELOGI("GeSession RunGraph finished");
  return ret;
}

Status GeSession::RunGraphWithStreamAsync(uint32_t graph_id, void *stream, const std::vector<gert::Tensor> &inputs,
                                          std::vector<gert::Tensor> &outputs) {
  RT2_PROFILING_SCOPE(gert::profiling::kUnknownName, gert::profiling::kStaticGraphExecute);
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Execute graph with stream async failed, session_id:%lu.",
                         GetSessionId());

  Status ret = inner_session->RunGraphWithStreamAsync(graph_id, stream, inputs, outputs);
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, FAILED,
                         "Run graph with stream async failed, error code:%u,"
                         " session_id:%lu, graph_id:%u, stream:%p.",
                         ret, GetSessionId(), graph_id, stream);
  return SUCCESS;
}

Status GeSession::RegisterCallBackFunc(const char *key, const RunCallback &callback) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  std::string str_key;
  if (key != nullptr) {
    str_key = key;
  }

  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "[Get][GeSession] failed, session_id:%lu.", GetSessionId());
  return inner_session->RegisterCallBackFunc(str_key, callback);
}

Status GeSession::LoadGraph(const uint32_t graph_id, const std::map<AscendString, AscendString> &options,
                            void *stream) const {
  GELOGD("Loading graph to session, graph_id: %u, session_id: %u, stream:%p .", graph_id, GetSessionId(), stream);
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Load graph failed, session_id:%lu.", GetSessionId());

  Status ret = inner_session->LoadGraph(graph_id, options, stream);
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, FAILED,
                         "Load graph failed, error code:%u, session_id:%lu, graph_id:%u.",
                         ret, GetSessionId(), graph_id);

  return ret;
}

// Run Graph Asynchronously
Status GeSession::RunGraphAsync(uint32_t graph_id, const std::vector<gert::Tensor> &inputs,
                                RunAsyncCallbackV2 callback) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "Run graph async failed, session_id:%lu.", GetSessionId());

  GRAPH_PROFILING_REG(gert::GeProfInfoType::kRunGraphAsync);
  GELOGI("start to run graph async, session_id: %lu, graph_id: %u, input size %zu", GetSessionId(), graph_id,
         inputs.size());

  GELOGI(
      "The callback function will not be checked. Please ensure that the implementation of the function is trusted,"
      " graph_id: %u", graph_id);

  Status ret = inner_session->RunGraphAsync(graph_id, inputs, callback);
  GE_CHK_BOOL_RET_STATUS(ret == SUCCESS, FAILED, "Run graph async failed, error code:%u, session_id:%lu, graph_id:%u.",
                         ret, GetSessionId(), graph_id);
  GELOGD("RunGraphAsync finished in GeSession, graph_id: %u,", graph_id);
  return SUCCESS;
}

bool GeSession::IsGraphNeedRebuild(uint32_t graph_id) {
  GRAPH_PROFILING_REG(gert::GeProfInfoType::kIsGraphNeedRebuild);
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return false;
  }

  GE_ASSERT_NOTNULL(impl_, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  if (inner_session == nullptr) {
    GELOGE(FAILED, "[Get][GeSession] failed, session_id:%lu.", GetSessionId());
    return true;
  }
  return inner_session->IsGraphNeedRebuild(graph_id);
}

uint64_t GeSession::GetSessionId() const {
  if (impl_) {
    return impl_->GetSessionId();
  } else {
    return 0;
  }
}

Status GeSession::CompileGraph(uint32_t graph_id) {
  return CompileGraph(graph_id, {});
}

Status GeSession::CompileGraph(uint32_t graph_id, const std::vector<ge::Tensor> &inputs) {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const auto inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_ASSERT_NOTNULL(inner_session, "[Get][GeSession]Failed, session_id:%lu.", GetSessionId());

  GELOGT(TRACE_INIT, "Start to compile graph, session_id:%lu, graph_id:%u, inputs size:%zu",
         GetSessionId(), graph_id, inputs.size());

  Status ret = inner_session->CompileGraph(graph_id, inputs);
  GE_ASSERT_SUCCESS(ret, "[Compile][Graph]Compile graph failed, error code:%u, session_id:%lu, graph_id:%u.",
      ret, GetSessionId(), graph_id);
  GELOGT(TRACE_STOP, "Compile graph success, session_id:%lu, graph_id:%u, inputs size:%zu",
         GetSessionId(), graph_id, inputs.size());
  return SUCCESS;
}

CompiledGraphSummaryPtr GeSession::GetCompiledGraphSummary(uint32_t graph_id) {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_ASSERT_NOTNULL(impl_, "GeSession construction incomplete (null impl pointer)");
  const auto inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_ASSERT_NOTNULL(inner_session, "[Get][GeSession]Failed, session_id:%lu.", GetSessionId());

  CompiledGraphSummaryPtr summary = nullptr;
  Status ret = inner_session->GetCompiledGraphSummary(graph_id, summary);
  GE_ASSERT_SUCCESS(ret, "[Get][Summary]Failed, error code:%u, session_id:%lu, graph_id:%u.",
                    ret, GetSessionId(), graph_id);
  return summary;
}

Status GeSession::SetGraphConstMemoryBase(uint32_t graph_id, const void *const memory, size_t size) {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  if (EnableSliceSchedule()) {
    GELOGE(UNSUPPORTED,
           "[Construct][GeSession]SetGraphConstMemoryBase unsupport slice scheduler currently, session_id:%lu, "
           "graph_id:%u, memory:%p, size:%zu",
           GetSessionId(), graph_id, memory, size);
    REPORT_INNER_ERR_MSG(
        "E19999",
        "SetGraphConstMemoryBase unsupport slice scheduler currently, session_id:%lu, graph_id:%u, memory:%p, size:%zu",
        GetSessionId(), graph_id, memory, size);
    return UNSUPPORTED;
  }

  const auto inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_ASSERT_NOTNULL(inner_session, "[Get][GeSession]Failed, session_id:%lu.", GetSessionId());

  const auto ret = inner_session->SetGraphConstMemoryBase(graph_id, memory, size);
  GE_ASSERT_SUCCESS(ret, "[Set][Memory]Failed, error code:%u, session_id:%lu, graph_id:%u, memory:%p, size:%zu", ret,
                    GetSessionId(), graph_id, memory, size);
  return SUCCESS;
}

Status GeSession::UpdateGraphFeatureMemoryBase(uint32_t graph_id, const void *const memory, size_t size) {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  if (EnableSliceSchedule()) {
    GELOGE(UNSUPPORTED,
           "[Construct][GeSession]UpdateGraphFeatureMemoryBase unsupport slice scheduler currently, session_id:%lu, "
           "graph_id:%u, memory:%p, size:%zu",
           GetSessionId(), graph_id, memory, size);
    REPORT_INNER_ERR_MSG("E19999",
                         "UpdateGraphFeatureMemoryBase unsupport slice scheduler currently, session_id:%lu, "
                         "graph_id:%u, memory:%p, size:%zu",
                         GetSessionId(), graph_id, memory, size);
    return UNSUPPORTED;
  }
  const auto inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_ASSERT_NOTNULL(inner_session, "[Get][GeSession]Failed, session_id:%lu.", GetSessionId());

  const auto ret = inner_session->UpdateGraphFeatureMemoryBase(graph_id, memory, size);
  GE_ASSERT_SUCCESS(ret, "[Update][Memory]Failed, error code:%u, session_id:%lu, graph_id:%u, memory:%p, size:%zu", ret,
                    GetSessionId(), graph_id, memory, size);
  return SUCCESS;
}

Status GeSession::SetGraphFixedFeatureMemoryBaseWithType(uint32_t graph_id, MemoryType type, const void *const memory,
                                                         size_t size) {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  if (EnableSliceSchedule()) {
    GELOGE(UNSUPPORTED,
           "[Construct][GeSession]SetGraphFixedFeatureMemoryBaseWithType unsupport slice scheduler currently, "
           "session_id:%lu, graph_id:%u, type:%d, memory:%p, size:%zu",
           GetSessionId(), graph_id, type, memory, size);
    REPORT_INNER_ERR_MSG("E19999",
                         "SetGraphFixedFeatureMemoryBaseWithType unsupport slice scheduler currently, session_id:%lu, "
                         "graph_id:%u, memory:%p, size:%zu",
                         GetSessionId(), graph_id, memory, size);
    return UNSUPPORTED;
  }
  const auto inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_ASSERT_NOTNULL(inner_session, "[Get][GeSession]Failed, session_id:%lu.", GetSessionId());

  const auto ret = inner_session->SetGraphFixedFeatureMemoryBase(graph_id, type, memory, size);
  GE_ASSERT_SUCCESS(ret,
                    "[Set][Memory]Failed, error code:%u, session_id:%lu, graph_id:%u, type:%d,"
                    " memory:%p, size:%zu",
                    ret, GetSessionId(), graph_id, type, memory, size);
  return SUCCESS;
}

Status GeSession::UpdateGraphRefreshableFeatureMemoryBase(uint32_t graph_id, const void *const memory, size_t size) {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  if (EnableSliceSchedule()) {
    GELOGE(UNSUPPORTED,
           "[Construct][GeSession]UpdateGraphRefreshableFeatureMemoryBase unsupport slice scheduler currently, "
           "session_id:%lu, graph_id:%u, memory:%p, size:%zu",
           GetSessionId(), graph_id, memory, size);
    REPORT_INNER_ERR_MSG("E19999",
                         "UpdateGraphRefreshableFeatureMemoryBase unsupport slice scheduler currently, session_id:%lu, "
                         "graph_id:%u, memory:%p, size:%zu",
                         GetSessionId(), graph_id, memory, size);
    return UNSUPPORTED;
  }
  const auto inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_ASSERT_NOTNULL(inner_session, "[Get][GeSession]Failed, session_id:%lu.", GetSessionId());

  const auto ret = inner_session->UpdateGraphRefreshableFeatureMemoryBase(graph_id, memory, size);
  GE_ASSERT_SUCCESS(ret, "[Update][Memory]Failed, error code:%u, session_id:%lu, graph_id:%u, memory:%p, size:%zu", ret,
                    GetSessionId(), graph_id, memory, size);
  return SUCCESS;
}

Status GeSession::RegisterExternalAllocator(const void *const stream, AllocatorPtr allocator) const {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "[Get][GeSession] failed, session_id:%lu.", GetSessionId());

  GE_CHK_STATUS_RET(inner_session->RegisterExternalAllocator(stream, allocator), "register external allocator failed");
  return SUCCESS;
}

Status GeSession::UnregisterExternalAllocator(const void *const stream) const {
  GE_ASSERT(g_ge_initialized, "[Construct][GeSession]Failed because lack GEInitialize call before.");
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "[Get][GeSession] failed, session_id:%lu.", GetSessionId());

  GE_CHK_STATUS_RET(inner_session->UnregisterExternalAllocator(stream), "unregister external allocator failed");
  return SUCCESS;
}

Status GetSessionMemInfo(const uint64_t session_id, uint64_t &var_size,
                         std::map<uint32_t, std::vector<uint64_t>> &graphs_mem_info) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "[Construct][GeSession]Failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Creating session failed because lack GEInitialize call before.");
    return FAILED;
  }

  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(session_id);
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "[Get][GeSession] failed, session_id:%lu.", session_id);

  auto &graph_manager = inner_session->getGraphManagerObj();
  GE_CHK_STATUS_RET(graph_manager.GetGraphsMemInfo(graphs_mem_info), "Get graphs memory info failed");
  const auto &var_manager = ge::VarManager::Instance(session_id);
  GE_CHECK_NOTNULL(var_manager);
  var_size = static_cast<uint64_t>(var_manager->GetVarMemSize(RT_MEMORY_HBM));
  GELOGD("GeSession memory info:var_size:%lu", var_size);
  return SUCCESS;
}

Status GeSession::GetCompiledModel(uint32_t graph_id, ModelBufferData &model_buffer) {
  if (!g_ge_initialized) {
    GELOGE(GE_CLI_GE_NOT_INITIALIZED, "Construct session failed because lack GEInitialize call before.");
    REPORT_INNER_ERR_MSG("E19999", "Construct session failed because lack GEInitialize call before.");
    return FAILED;
  }
  GE_CHK_BOOL_RET_STATUS(impl_ != nullptr, FAILED, "GeSession construction incomplete (null impl pointer)");
  const InnerGeSessionPtr inner_session = g_ge_session_manager->GetSession(GetSessionId());
  GE_CHK_BOOL_RET_STATUS(inner_session != nullptr, FAILED, "[Get][GeSession] failed, session_id:%lu.", GetSessionId());
  return inner_session->GetCompiledModel(graph_id, model_buffer);
}
}  // namespace ge

extern "C" {
ge::Status GetRegisteredIrDef(const char *op_type, std::vector<std::pair<ge::AscendString, ge::AscendString>> &inputs,
                              std::vector<std::pair<ge::AscendString, ge::AscendString>> &outputs,
                              std::vector<std::pair<ge::AscendString, ge::AscendString>> &attrs) {
  GE_ASSERT_NOTNULL(op_type);
  const auto op = ge::OperatorFactory::CreateOperator("_", op_type);
  GE_WARN_ASSERT(!op.IsEmpty(), "No operator found for type: %s", op_type);
  const auto desc = ge::OpDescUtils::GetOpDescFromOperator(op);

  static const auto kInputTypeString = []() {
    std::map<ge::IrInputType, ge::AscendString> typeStr;
    typeStr[ge::IrInputType::kIrInputRequired] = "required";
    typeStr[ge::IrInputType::kIrInputOptional] = "optional";
    typeStr[ge::IrInputType::kIrInputDynamic] = "dynamic";
    return typeStr;
  }();

  static const auto kOutputTypeString = []() {
    std::map<ge::IrOutputType, ge::AscendString> typeStr;
    typeStr[ge::IrOutputType::kIrOutputRequired] = "required";
    typeStr[ge::IrOutputType::kIrOutputDynamic] = "dynamic";
    return typeStr;
  }();

  GE_ASSERT_NOTNULL(desc, "Failed to get OpDesc from operator: %s", op_type);
  for (const auto &name2type : desc->GetIrInputs()) {
    auto iter = kInputTypeString.find(name2type.second);
    GE_ASSERT(iter != kInputTypeString.end(), "Unknown input type: %d for operator: %s", name2type.second, op_type);
    inputs.emplace_back(ConvertToAscendString(name2type.first), iter->second);
  }
  for (const auto &name2type : desc->GetIrOutputs()) {
    auto iter = kOutputTypeString.find(name2type.second);
    GE_ASSERT(iter != kOutputTypeString.end(), "Unknown output type: %d for operator: %s", name2type.second, op_type);
    outputs.emplace_back(ConvertToAscendString(name2type.first), iter->second);
  }

  std::map<ge::AscendString, ge::AscendString> attrs_and_types;
  GE_ASSERT_GRAPH_SUCCESS(op.GetAllIrAttrNamesAndTypes(attrs_and_types),
                          "Failed to get attr names and types for operator: %s", op_type);
  for (const auto &attr : desc->GetIrAttrNames()) {
    attrs.emplace_back(ConvertToAscendString(attr), attrs_and_types[ConvertToAscendString(attr)]);
  }
  return ge::SUCCESS;
}
}
