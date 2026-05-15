/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/dump_config.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/debug/log.h"
#include "common/ge_common/util.h"
#include <ctime>

namespace ge {
namespace dump {

namespace {
// 本地实现时间格式化函数，避免依赖外部 so
// format: 20171122042550
std::string GetCurrentTimeStr() {
  const std::time_t now = std::time(nullptr);
  const std::tm *const ptm = std::localtime(&now);
  if (ptm == nullptr) {
    GELOGE(ge::FAILED, "localtime failed, return empty time string");
    return "";
  }

  constexpr int32_t kTimeBufferLen = 32;
  char buffer[kTimeBufferLen + 1] = {};
  (void)std::strftime(buffer, static_cast<size_t>(kTimeBufferLen), "%Y%m%d%H%M%S", ptm);
  return std::string(buffer);
}
}  // namespace

DumpConfig& DumpConfig::Instance() {
  static DumpConfig instance;
  return instance;
}

bool DumpConfig::IsDataDumpEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_dump_enabled_;
}

bool DumpConfig::IsExceptionDumpEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return exception_dump_enabled_;
}

bool DumpConfig::IsOverflowDumpEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return overflow_dump_enabled_;
}

void DumpConfig::SetDataDumpEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_dump_enabled_ = enabled;
}

void DumpConfig::SetExceptionDumpEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  exception_dump_enabled_ = enabled;
}

void DumpConfig::SetOverflowDumpEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  overflow_dump_enabled_ = enabled;
}

const std::string& DumpConfig::GetDumpPath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_path_;
}

const std::string& DumpConfig::GetDumpStep() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_step_;
}

const std::string& DumpConfig::GetDumpMode() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_mode_;
}

const std::string& DumpConfig::GetDumpData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_data_;
}

const std::unordered_set<std::string>& DumpConfig::GetOpList() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return op_list_;
}

void DumpConfig::SetDumpPath(const std::string& path) {
  std::lock_guard<std::mutex> lock(mutex_);
  dump_path_ = path;
}

void DumpConfig::SetDumpStep(const std::string& step) {
  std::lock_guard<std::mutex> lock(mutex_);
  dump_step_ = step;
}

void DumpConfig::SetDumpMode(const std::string& mode) {
  std::lock_guard<std::mutex> lock(mutex_);
  dump_mode_ = mode;
}

void DumpConfig::SetDumpData(const std::string& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  dump_data_ = data;
}

void DumpConfig::SetOpList(const std::unordered_set<std::string>& op_list) {
  std::lock_guard<std::mutex> lock(mutex_);
  op_list_ = op_list;
}

void DumpConfig::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  data_dump_enabled_ = false;
  exception_dump_enabled_ = false;
  overflow_dump_enabled_ = false;
  dump_path_.clear();
  dump_step_.clear();
  dump_mode_.clear();
  dump_data_.clear();
  op_list_.clear();
  dump_level_.clear();
  dump_status_.clear();
  dump_op_switch_.clear();
  dump_debug_.clear();
  dump_stats_.clear();
  dump_scene_.clear();
  model_dump_config_list_.clear();
  op_debug_mode_ = 0U;
}

const std::string& DumpConfig::GetDumpLevel() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_level_;
}

const std::string& DumpConfig::GetDumpStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_status_;
}

const std::string& DumpConfig::GetDumpOpSwitch() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_op_switch_;
}

const std::string& DumpConfig::GetDumpDebug() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_debug_;
}

uint32_t DumpConfig::GetOpDebugMode() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return op_debug_mode_;
}

const std::vector<std::string>& DumpConfig::GetDumpStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_stats_;
}

const std::string& DumpConfig::GetDumpScene() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dump_scene_;
}

const std::vector<ModelDumpConfig>& DumpConfig::GetModelDumpConfigList() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return model_dump_config_list_;
}

bool DumpConfig::NeedDump() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if ((!model_dump_config_list_.empty()) ||
      (dump_op_switch_ == GE_DUMP_STATUS_ON) ||
      (dump_debug_ == GE_DUMP_STATUS_ON) ||
      (!dump_scene_.empty())) {
    return true;
  }
  return false;
}

bool DumpConfig::IsOpNeedDump(const std::string& op_name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  // 如果没有配置 dump list，默认返回 true（由全局开关控制是否真的 dump）
  if (model_dump_config_list_.empty()) {
    return true;
  }

  // 如果配置了 dump list，按照以下逻辑判断
  for (const auto& model_config : model_dump_config_list_) {
    // layers 为空，表示该模型所有 op 都需要 dump
    if (model_config.layers.empty() && model_config.watcher_nodes.empty()) {
      return true;
    }

    // 遍历 layers 匹配（前缀匹配）
    for (const auto& layer : model_config.layers) {
      if (op_name == layer) {
        return true;
      }
    }

    // watcher nodes 匹配（精确匹配）
    for (const auto& node : model_config.watcher_nodes) {
      if (op_name == node) {
        return true;
      }
    }
  }

  // 有 dump list，但当前 op 不在列表中
  return false;
}

Status DumpConfig::ParseAndValidate(const char* dumpData, int32_t size) {
  GELOGI("Start to parse and validate dump config, size: %d", size);
  if (dumpData == nullptr || size <= 0) {
    GELOGI("Dump data is null or empty, no dump config to parse");
    return SUCCESS;
  }

  std::string jsonStr(dumpData, size);
  nlohmann::json js;
  try {
    js = nlohmann::json::parse(jsonStr);
  } catch (...) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Parse][DumpConfig]Failed to parse dump json string");
    REPORT_INNER_ERR_MSG("E19999", "Failed to parse dump json string");
    return FAILED;
  }

  if (!IsValidDumpConfig(js)) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Check][DumpConfig]Dump config validation failed");
    return FAILED;
  }

  Status ret = ParseDumpConfigFromJson(js);
  if (ret != SUCCESS) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Parse][DumpConfig]Failed to parse dump config from json");
    return ret;
  }

  // 检查 OM2 暂不支持的配置项并打印 warning
  const nlohmann::json& jsDumpConfig = js.contains(GE_DUMP) ? js[GE_DUMP] : js;
  CheckUnsupportedConfigs(jsDumpConfig);

  GELOGI("Parse and validate dump config successfully");
  return SUCCESS;
}

bool DumpConfig::IsValidDumpConfig(const nlohmann::json& js) const {
  GELOGI("Start to execute IsValidDumpConfig.");
  if (!js.contains(GE_DUMP)) {
    GELOGI("no dump item, no need to do dump!");
    return true;
  }

  const nlohmann::json& jsDumpConfig = js[GE_DUMP];

  if (jsDumpConfig.contains(GE_DUMP_SCENE)) {
    std::string dumpScene;
    return CheckDumpSceneSwitch(jsDumpConfig, dumpScene);
  }

  if (IsDumpDebugEnabled(jsDumpConfig)) {
    return DumpDebugCheck(jsDumpConfig);
  }

  return ValidateNormalDumpConfig(jsDumpConfig);
}

bool DumpConfig::IsDumpDebugEnabled(const nlohmann::json& jsDumpConfig) {
  std::string dumpDebug = GE_DUMP_DEBUG_DEFAULT;
  if (jsDumpConfig.contains(GE_DUMP_DEBUG)) {
    dumpDebug = jsDumpConfig[GE_DUMP_DEBUG].get<std::string>();
  }
  return (dumpDebug == GE_DUMP_STATUS_ON);
}

bool DumpConfig::ValidateNormalDumpConfig(const nlohmann::json& jsDumpConfig) {
  if (!ValidateDumpPath(jsDumpConfig)) {
    return false;
  }

  if (!ValidateDumpMode(jsDumpConfig)) {
    return false;
  }

  if (!ValidateDumpLevel(jsDumpConfig)) {
    return false;
  }

  return ValidateOtherDumpConfigs(jsDumpConfig);
}

bool DumpConfig::ValidateDumpPath(const nlohmann::json& jsDumpConfig) {
  if (!jsDumpConfig.contains(GE_DUMP_PATH)) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Check][DumpConfig]dump_path field in dump config does not exist");
    REPORT_INNER_ERR_MSG("E19999", "dump_path field in dump config does not exist");
    return false;
  }

  const std::string dumpPath = jsDumpConfig[GE_DUMP_PATH].get<std::string>();
  if (dumpPath.empty()) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Check][DumpConfig]dump_path field is null in config");
    REPORT_INNER_ERR_MSG("E19999", "dump_path field is null in config");
    return false;
  }

  return CheckDumpPath(jsDumpConfig);
}

bool DumpConfig::ValidateDumpMode(const nlohmann::json& jsDumpConfig) {
  std::string dumpMode = GE_DUMP_MODE_DEFAULT;
  if (jsDumpConfig.contains(GE_DUMP_MODE)) {
    dumpMode = jsDumpConfig[GE_DUMP_MODE].get<std::string>();
  }

  const std::vector<std::string> validModes = {
      GE_DUMP_MODE_INPUT, GE_DUMP_MODE_OUTPUT, GE_DUMP_MODE_ALL
  };

  if (std::find(validModes.begin(), validModes.end(), dumpMode) == validModes.end()) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG,
           "[Check][DumpConfig]dump_mode value[%s] error in config, only supports input/output/all",
           dumpMode.c_str());
    REPORT_INNER_ERR_MSG("E19999",
                        "dump_mode value[%s] error in config, only supports input/output/all",
                        dumpMode.c_str());
    return false;
  }
  GELOGD("dump_mode value[%s] is valid", dumpMode.c_str());
  return true;
}

bool DumpConfig::ValidateDumpLevel(const nlohmann::json& jsDumpConfig) {
  std::string dumpLevel = GE_DUMP_LEVEL_DEFAULT;
  if (jsDumpConfig.contains(GE_DUMP_LEVEL)) {
    dumpLevel = jsDumpConfig[GE_DUMP_LEVEL].get<std::string>();
  }

  const std::vector<std::string> validLevels = {
      GE_DUMP_LEVEL_OP, GE_DUMP_LEVEL_KERNEL, GE_DUMP_LEVEL_ALL
  };

  if (std::find(validLevels.begin(), validLevels.end(), dumpLevel) == validLevels.end()) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG,
           "[Check][DumpConfig]dump_level value[%s] error in config, only supports op/kernel/all",
           dumpLevel.c_str());
    REPORT_INNER_ERR_MSG("E19999",
                        "dump_level value[%s] error in config, only supports op/kernel/all",
                        dumpLevel.c_str());
    return false;
  }
  GELOGD("dump_level value[%s] is valid", dumpLevel.c_str());
  return true;
}

bool DumpConfig::ValidateOtherDumpConfigs(const nlohmann::json& jsDumpConfig) {
  return CheckDumpStep(jsDumpConfig) &&
         CheckDumplist(jsDumpConfig, GetDumpLevel(jsDumpConfig)) &&
         DumpStatsCheck(jsDumpConfig);
}

std::string DumpConfig::GetDumpLevel(const nlohmann::json& jsDumpConfig) {
  if (jsDumpConfig.contains(GE_DUMP_LEVEL)) {
    return jsDumpConfig[GE_DUMP_LEVEL].get<std::string>();
  }
  return GE_DUMP_LEVEL_DEFAULT;
}

void DumpConfig::CheckUnsupportedConfigs(const nlohmann::json& jsDumpConfig) {
  if (jsDumpConfig.contains(GE_DUMP_OP_SWITCH)) {
    GELOGW("[OM2 Dump] dump_op_switch is not supported in OM2 dump mode, configuration will be ignored");
  }
  if (jsDumpConfig.contains(GE_DUMP_LIST)) {
    const auto& dumpList = jsDumpConfig[GE_DUMP_LIST];
    if (dumpList.is_array() && !dumpList.empty()) {
      for (const auto& modelJson : dumpList) {
        if (modelJson.contains(GE_DUMP_OPTYPE_BLACKLIST)) {
          GELOGW("[OM2 Dump] optype_blacklist in dump_list is not supported in OM2 dump mode, configuration will be ignored");
        }
        if (modelJson.contains(GE_DUMP_OPNAME_BLACKLIST)) {
          GELOGW("[OM2 Dump] opname_blacklist in dump_list is not supported in OM2 dump mode, configuration will be ignored");
        }
        if (modelJson.contains(GE_DUMP_OPNAME_RANGE)) {
          GELOGW("[OM2 Dump] opname_range in dump_list is not supported in OM2 dump mode, configuration will be ignored");
        }
      }
    }
  }
  if (jsDumpConfig.contains(GE_DUMP_SCENE)) {
    std::string dumpScene = jsDumpConfig[GE_DUMP_SCENE].get<std::string>();
    if ((dumpScene == GE_DUMP_EXCEPTION_AIC_ERR_BRIEF) || (dumpScene == GE_DUMP_LITE_EXCEPTION)) {
      GELOGW("[OM2 Dump] dump_scene value '%s' (L0 exception dump) is not supported in OM2, only L1 (aic_err_norm_dump) and L2 (aic_err_detail_dump) exception dump are supported", dumpScene.c_str());
    }
  }
}

bool DumpConfig::CheckDumpSceneSwitch(const nlohmann::json& jsDumpConfig, std::string& dumpScene) {
  if (!jsDumpConfig.contains(GE_DUMP_SCENE)) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Check][DumpConfig]dump_scene field does not exist");
    return false;
  }

  dumpScene = jsDumpConfig[GE_DUMP_SCENE].get<std::string>();
  const std::vector<std::string> validScenes = {
      GE_DUMP_LITE_EXCEPTION, GE_DUMP_EXCEPTION_AIC_ERR_BRIEF,
      GE_DUMP_EXCEPTION_AIC_ERR_NORM, GE_DUMP_EXCEPTION_AIC_ERR_DETAIL,
      GE_DUMP_SCENE_WATCHER
  };

  if (std::find(validScenes.begin(), validScenes.end(), dumpScene) == validScenes.end()) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Check][DumpConfig]dump_scene value[%s] is invalid",
           dumpScene.c_str());
    return false;
  }
  GELOGD("dump_scene value[%s] is valid", dumpScene.c_str());
  return true;
}

bool DumpConfig::CheckDumpPath(const nlohmann::json& jsDumpConfig) {
  (void)jsDumpConfig;
  return true;
}

bool DumpConfig::CheckDumpStep(const nlohmann::json& jsDumpConfig) {
  (void)jsDumpConfig;
  return true;
}

bool DumpConfig::CheckDumplist(const nlohmann::json& jsDumpConfig, const std::string& dumpLevel) {
  (void)jsDumpConfig;
  (void)dumpLevel;
  return true;
}

bool DumpConfig::CheckDumpOpSwitch(const std::string& dumpOpSwitch) {
  const std::vector<std::string> validSwitches = {GE_DUMP_STATUS_ON, GE_DUMP_STATUS_OFF};
  return std::find(validSwitches.begin(), validSwitches.end(), dumpOpSwitch) != validSwitches.end();
}

bool DumpConfig::CheckIpAddress(const DumpConfig& config) {
  (void)config;
  return true;
}

bool DumpConfig::ValidateIpAddress(const std::string& ipAddress) {
  (void)ipAddress;
  return true;
}

bool DumpConfig::IsDumpPathValid(const std::string& dumpPath) {
  return dumpPath.size() <= MAX_DUMP_PATH_LENGTH;
}

bool DumpConfig::DumpDebugCheck(const nlohmann::json& jsDumpConfig) {
  (void)jsDumpConfig;
  return true;
}

bool DumpConfig::DumpStatsCheck(const nlohmann::json& jsDumpConfig) {
  (void)jsDumpConfig;
  return true;
}

Status DumpConfig::ParseDumpConfigFromJson(const nlohmann::json& dumpJson) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!dumpJson.contains(GE_DUMP)) {
    GELOGI("no dump item, no need to parse dump config!");
    return SUCCESS;
  }

  const nlohmann::json& jsDumpConfig = dumpJson[GE_DUMP];
  ParseBasicConfigurations(jsDumpConfig);
  ParseComplexConfigs(jsDumpConfig);

  // 对齐 V1：dump_path 末尾加上时间戳
  if (!dump_path_.empty()) {
    // 确保路径以 / 结尾
    if (dump_path_[dump_path_.size() - 1U] != '/') {
      dump_path_ += "/";
    }
    dump_path_ += GetCurrentTimeStr() + "/";
  }

  // 根据配置设置开关：
  // data_dump_enabled_ 对应 v1 IsDumpOpen()，只由 dump_status:on 或 dump_list 控制
  // overflow dump (dump_debug: on) 即使配置了 dump_path，也不开启 data_dump
  if ((dump_status_ == GE_DUMP_STATUS_ON) || !model_dump_config_list_.empty()) {
    data_dump_enabled_ = true;
    GELOGI("Data dump enabled, dump_status: %s, model config count: %zu",
           dump_status_.c_str(), model_dump_config_list_.size());
  }
  if (!dump_scene_.empty()) {
    exception_dump_enabled_ = true;
    GELOGI("Exception dump enabled, dump_scene: %s", dump_scene_.c_str());
  }
  if (dump_debug_ == GE_DUMP_STATUS_ON) {
    overflow_dump_enabled_ = true;
    GELOGI("Overflow dump enabled, dump_path: %s", dump_path_.c_str());
  }

  GELOGI("Parse dump config from json successfully, dump_mode: %s, dump_level: %s, dump_step: %s",
         dump_mode_.c_str(), dump_level_.c_str(), dump_step_.c_str());
  return SUCCESS;
}

void DumpConfig::ParseBasicConfigurations(const nlohmann::json& jsDumpConfig) {
  dump_path_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_PATH, "");
  dump_mode_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_MODE, GE_DUMP_MODE_DEFAULT);
  dump_status_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_STATUS, GE_DUMP_STATUS_DEFAULT);
  dump_op_switch_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_OP_SWITCH, GE_DUMP_OP_SWITCH_DEFAULT);
  dump_debug_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_DEBUG, GE_DUMP_DEBUG_DEFAULT);
  dump_step_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_STEP, GE_DUMP_STEP_DEFAULT);
  dump_data_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_DATA, GE_DUMP_DATA_DEFAULT);
  dump_level_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_LEVEL, GE_DUMP_LEVEL_DEFAULT);
  dump_scene_ = GetConfigWithDefault(jsDumpConfig, GE_DUMP_SCENE, GE_DUMP_SCENE_DEFAULT);

  // 对齐 v1 DumpManager::SetDumpDebugConf：dump_debug=on 时启用全部溢出检测，
  // 同时将 dump_status 强制置为 off（对齐 v1 HandleDumpDebugConfig 行为），
  // 避免 dump_debug 场景下误触发 data dump。
  if (dump_debug_ == GE_DUMP_STATUS_ON) {
    op_debug_mode_ = kAllOverflow;
    dump_status_ = GE_DUMP_STATUS_OFF;
  } else {
    op_debug_mode_ = 0U;
  }
}

void DumpConfig::ParseComplexConfigs(const nlohmann::json& jsDumpConfig) {
  if (jsDumpConfig.contains(GE_DUMP_STATS) && jsDumpConfig[GE_DUMP_STATS].is_array()) {
    ParseStringArray(jsDumpConfig[GE_DUMP_STATS], dump_stats_);
  }

  if (jsDumpConfig.contains(GE_DUMP_LIST) && jsDumpConfig[GE_DUMP_LIST].is_array()) {
    ParseModelDumpConfigList(jsDumpConfig[GE_DUMP_LIST], model_dump_config_list_);
  }
}

std::string DumpConfig::GetConfigWithDefault(const nlohmann::json& json,
                                             const std::string& key,
                                             const std::string& defaultValue) {
  if (json.contains(key)) {
    return json[key].get<std::string>();
  }
  return defaultValue;
}

void DumpConfig::ParseStringArray(const nlohmann::json& jsonArray, std::vector<std::string>& output) {
  output.clear();
  for (const auto& item : jsonArray) {
    output.push_back(item.get<std::string>());
  }
}

void DumpConfig::ParseModelDumpConfigList(const nlohmann::json& jsonArray, std::vector<ModelDumpConfig>& output) {
  output.clear();
  for (const auto& modelJson : jsonArray) {
    ModelDumpConfig modelConfig;
    if (ParseModelDumpConfig(modelJson, modelConfig)) {
      output.push_back(modelConfig);
    }
  }
}

bool DumpConfig::ParseModelDumpConfig(const nlohmann::json& modelJson, ModelDumpConfig& modelConfig) {
  if (!CheckDumpModelConfig(modelJson)) {
    return false;
  }

  if (modelJson.contains(GE_DUMP_MODEL_NAME)) {
    modelConfig.model_name = modelJson[GE_DUMP_MODEL_NAME].get<std::string>();
  }

  if (modelJson.contains(GE_DUMP_LAYER) && modelJson[GE_DUMP_LAYER].is_array()) {
    modelConfig.layers.clear();
    for (const auto& layer : modelJson[GE_DUMP_LAYER]) {
      modelConfig.layers.push_back(layer.get<std::string>());
    }
  }

  if (modelJson.contains(GE_DUMP_WATCHER_NODES) && modelJson[GE_DUMP_WATCHER_NODES].is_array()) {
    modelConfig.watcher_nodes.clear();
    for (const auto& node : modelJson[GE_DUMP_WATCHER_NODES]) {
      modelConfig.watcher_nodes.push_back(node.get<std::string>());
    }
  }

  if (modelJson.contains(GE_DUMP_OPTYPE_BLACKLIST) && modelJson[GE_DUMP_OPTYPE_BLACKLIST].is_array()) {
    modelConfig.optype_blacklist.clear();
    for (const auto& blacklistJson : modelJson[GE_DUMP_OPTYPE_BLACKLIST]) {
      DumpBlacklist blacklist;
      ParseBlacklist(blacklistJson, blacklist);
      modelConfig.optype_blacklist.push_back(blacklist);
    }
  }

  if (modelJson.contains(GE_DUMP_OPNAME_BLACKLIST) && modelJson[GE_DUMP_OPNAME_BLACKLIST].is_array()) {
    modelConfig.opname_blacklist.clear();
    for (const auto& blacklistJson : modelJson[GE_DUMP_OPNAME_BLACKLIST]) {
      DumpBlacklist blacklist;
      ParseBlacklist(blacklistJson, blacklist);
      modelConfig.opname_blacklist.push_back(blacklist);
    }
  }

  if (modelJson.contains(GE_DUMP_OPNAME_RANGE) && modelJson[GE_DUMP_OPNAME_RANGE].is_array()) {
    modelConfig.dump_op_ranges.clear();
    for (const auto& rangeJson : modelJson[GE_DUMP_OPNAME_RANGE]) {
      if (rangeJson.contains(GE_DUMP_OPNAME_RANGE_BEGIN) && rangeJson.contains(GE_DUMP_OPNAME_RANGE_END)) {
        std::string begin = rangeJson[GE_DUMP_OPNAME_RANGE_BEGIN].get<std::string>();
        std::string end = rangeJson[GE_DUMP_OPNAME_RANGE_END].get<std::string>();
        modelConfig.dump_op_ranges.emplace_back(begin, end);
      }
    }
  }

  return true;
}

bool DumpConfig::CheckDumpModelConfig(const nlohmann::json& modelJson) {
  (void)modelJson;
  return true;
}

void DumpConfig::ParseBlacklist(const nlohmann::json& blacklistJson, DumpBlacklist& blacklist) {
  if (blacklistJson.contains(GE_DUMP_BLACKLIST_NAME)) {
    blacklist.name = blacklistJson[GE_DUMP_BLACKLIST_NAME].get<std::string>();
  }

  if (blacklistJson.contains(GE_DUMP_BLACKLIST_POS) && blacklistJson[GE_DUMP_BLACKLIST_POS].is_array()) {
    blacklist.pos.clear();
    for (const auto& pos : blacklistJson[GE_DUMP_BLACKLIST_POS]) {
      blacklist.pos.push_back(pos.get<std::string>());
    }
  }
}

std::vector<std::string> DumpConfig::Split(const std::string &str, const char_t * const delimiter) {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = str.find(delimiter);
  while (end != std::string::npos) {
    result.push_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }
  result.push_back(str.substr(start));
  return result;
}

}  // namespace dump
}  // namespace ge
