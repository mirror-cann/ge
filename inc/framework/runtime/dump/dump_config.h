/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_DUMP_CONFIG_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_DUMP_CONFIG_H_

#include <cstdint>
#include <string>
#include <unordered_set>
#include <mutex>
#include <vector>
#include "common/ge_common/ge_types.h"
#include "nlohmann/json.hpp"

namespace ge {
namespace dump {

const uint32_t ACL_GE_INVALID_DUMP_CONFIG = 100009U;
constexpr int32_t MAX_DUMP_PATH_LENGTH = 512;
constexpr int32_t MAX_IPV4_ADDRESS_LENGTH = 4;
constexpr int32_t MAX_IPV4_ADDRESS_VALUE = 255;
constexpr int32_t DECIMAL = 10;
const uint64_t AIC_ERR_BRIEF_DUMP_BIT = 0x4;
const uint64_t AIC_ERR_NORM_DUMP_BIT = 0x8;

const std::string GE_DUMP_MODE_INPUT = "input";
const std::string GE_DUMP_MODE_OUTPUT = "output";
const std::string GE_DUMP_MODE_ALL = "all";
const std::string GE_DUMP_STATUS_ON = "on";
const std::string GE_DUMP_STATUS_OFF = "off";
const std::string GE_DUMP_LEVEL_OP = "op";
const std::string GE_DUMP_LEVEL_KERNEL = "kernel";
const std::string GE_DUMP_LEVEL_ALL = "all";
const std::string GE_DUMP = "dump";

constexpr uint32_t kAicoreOverflow = 0x1U;
constexpr uint32_t kAtomicOverflow = 0x2U;
constexpr uint32_t kAllOverflow = kAicoreOverflow | kAtomicOverflow;

const std::string GE_DUMP_PATH = "dump_path";
const std::string GE_DUMP_MODE = "dump_mode";
const std::string GE_DUMP_STATUS = "dump_status";
const std::string GE_DUMP_OP_SWITCH = "dump_op_switch";
const std::string GE_DUMP_DEBUG = "dump_debug";
const std::string GE_DUMP_STEP = "dump_step";
const std::string GE_DUMP_DATA = "dump_data";
const std::string GE_DUMP_LEVEL = "dump_level";
const std::string GE_DUMP_STATS = "dump_stats";
const std::string GE_DUMP_LIST = "dump_list";
const std::string GE_DUMP_MODEL_NAME = "model_name";
const std::string GE_DUMP_LAYER = "layer";
const std::string GE_DUMP_WATCHER_NODES = "watcher_nodes";
const std::string GE_DUMP_OPTYPE_BLACKLIST = "optype_blacklist";
const std::string GE_DUMP_OPNAME_BLACKLIST = "opname_blacklist";
const std::string GE_DUMP_BLACKLIST_NAME = "name";
const std::string GE_DUMP_BLACKLIST_POS = "pos";
const std::string GE_DUMP_OPNAME_RANGE = "opname_range";
const std::string GE_DUMP_OPNAME_RANGE_BEGIN = "begin";
const std::string GE_DUMP_OPNAME_RANGE_END = "end";
const std::string GE_DUMP_SCENE = "dump_scene";
const std::string GE_DUMP_LITE_EXCEPTION = "lite_exception";
const std::string GE_DUMP_EXCEPTION_AIC_ERR_BRIEF = "aic_err_brief_dump";
const std::string GE_DUMP_EXCEPTION_AIC_ERR_NORM = "aic_err_norm_dump";
const std::string GE_DUMP_EXCEPTION_AIC_ERR_DETAIL = "aic_err_detail_dump";
const std::string GE_DUMP_SCENE_WATCHER = "watcher";

const std::string GE_DUMP_MODE_DEFAULT = "output";
const std::string GE_DUMP_STATUS_DEFAULT = "on";
const std::string GE_DUMP_OP_SWITCH_DEFAULT = "off";
const std::string GE_DUMP_DEBUG_DEFAULT = "off";
const std::string GE_DUMP_STEP_DEFAULT = "";
const std::string GE_DUMP_DATA_DEFAULT = "tensor";
const std::string GE_DUMP_LEVEL_DEFAULT = "all";
const std::string GE_DUMP_SCENE_DEFAULT = "";

class DumpConfig {
 public:
  static DumpConfig &Instance();

  // 开关
  bool IsDataDumpEnabled() const;
  bool IsExceptionDumpEnabled() const;
  bool IsOverflowDumpEnabled() const;

  void SetDataDumpEnabled(bool enabled);
  void SetExceptionDumpEnabled(bool enabled);
  void SetOverflowDumpEnabled(bool enabled);

  // 配置项
  const std::string &GetDumpPath() const;
  const std::string &GetDumpStep() const;
  const std::string &GetDumpMode() const;
  const std::string &GetDumpData() const;
  const std::unordered_set<std::string> &GetOpList() const;

  void SetDumpPath(const std::string &path);
  void SetDumpStep(const std::string &step);
  void SetDumpMode(const std::string &mode);
  void SetDumpData(const std::string &data);
  void SetOpList(const std::unordered_set<std::string> &op_list);

  // 重置配置
  void Reset();

  // 新增：完整配置解析与校验
  Status ParseAndValidate(const char *dumpData, int32_t size);
  bool IsValidDumpConfig(const nlohmann::json &js) const;

  // 新增：完整 Dump 支持的配置获取接口
  const std::string &GetDumpLevel() const;
  const std::string &GetDumpStatus() const;
  const std::string &GetDumpOpSwitch() const;
  const std::string &GetDumpDebug() const;
  uint32_t GetOpDebugMode() const;
  const std::vector<std::string> &GetDumpStats() const;
  const std::string &GetDumpScene() const;
  const std::vector<ModelDumpConfig> &GetModelDumpConfigList() const;

  // 配置是否需要 Dump
  bool NeedDump() const;

  // 判断某个算子是否需要 dump
  bool IsOpNeedDump(const std::string &op_name) const;

 private:
  DumpConfig() = default;
  ~DumpConfig() = default;

  // 检查 OM2 不支持的配置项并打印 warning
  static void CheckUnsupportedConfigs(const nlohmann::json &jsDumpConfig);

  // 校验辅助函数
  static bool CheckDumpSceneSwitch(const nlohmann::json &jsDumpConfig, std::string &dumpScene);
  static bool CheckDumpPath(const nlohmann::json &jsDumpConfig);
  static bool CheckDumpStep(const nlohmann::json &jsDumpConfig);
  static bool CheckDumplist(const nlohmann::json &jsDumpConfig, const std::string &dumpLevel);
  static bool CheckDumpOpSwitch(const std::string &dumpOpSwitch);
  static bool CheckIpAddress(const DumpConfig &config);
  static bool ValidateIpAddress(const std::string &ipAddress);
  static bool IsDumpPathValid(const std::string &dumpPath);
  static bool DumpDebugCheck(const nlohmann::json &jsDumpConfig);
  static bool DumpStatsCheck(const nlohmann::json &jsDumpConfig);
  static bool IsDumpDebugEnabled(const nlohmann::json &jsDumpConfig);
  static bool ValidateNormalDumpConfig(const nlohmann::json &jsDumpConfig);
  static bool ValidateDumpPath(const nlohmann::json &jsDumpConfig);
  static bool ValidateDumpMode(const nlohmann::json &jsDumpConfig);
  static bool ValidateDumpLevel(const nlohmann::json &jsDumpConfig);
  static bool ValidateOtherDumpConfigs(const nlohmann::json &jsDumpConfig);
  static std::string GetDumpLevel(const nlohmann::json &jsDumpConfig);
  static bool CheckDumpModelConfig(const nlohmann::json &modelJson);

  // 解析辅助函数
  Status ParseDumpConfigFromJson(const nlohmann::json &dumpJson);
  void ParseBasicConfigurations(const nlohmann::json &dumpJson);
  void ParseComplexConfigs(const nlohmann::json &dumpJson);
  static bool ParseModelDumpConfig(const nlohmann::json &modelJson, ModelDumpConfig &modelConfig);
  static void ParseBlacklist(const nlohmann::json &blacklistJson, DumpBlacklist &blacklist);
  static void ParseStringArray(const nlohmann::json &jsonArray, std::vector<std::string> &output);
  static void ParseModelDumpConfigList(const nlohmann::json &jsonArray, std::vector<ModelDumpConfig> &output);
  static std::string GetConfigWithDefault(const nlohmann::json &json, const std::string &key,
                                          const std::string &defaultValue);
  static std::vector<std::string> Split(const std::string &str, const char_t *const delimiter);

  mutable std::mutex mutex_;

  bool data_dump_enabled_ = false;
  bool exception_dump_enabled_ = false;
  bool overflow_dump_enabled_ = false;

  std::string dump_path_;
  std::string dump_step_;
  std::string dump_mode_;
  std::string dump_data_;
  std::unordered_set<std::string> op_list_;

  // 新增：扩展配置字段
  std::string dump_level_;
  std::string dump_status_;
  std::string dump_op_switch_;
  std::string dump_debug_;
  uint32_t op_debug_mode_ = 0U;
  std::vector<std::string> dump_stats_;
  std::string dump_scene_;  // 对应原始 dump_exception
  std::vector<ModelDumpConfig> model_dump_config_list_;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_COMMON_DUMP_DUMP_CONFIG_H_
