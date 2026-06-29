/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "main_impl.h"
#include "external/ge_common/ge_common_api_types.h"

#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <unordered_map>

#include "graph_metadef/common/plugin/plugin_manager.h"
#include "framework/common/util.h"
#include "framework/generator/ge_generator.h"
#include "framework/omg/omg.h"
#include "framework/omg/parser/parser_factory.h"
#include "atc_flags.h"
#include "atc_option_map.h"
#include "cmd_flag_info.h"
#include "base/err_msg.h"
#include "base/err_mgr.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/type_utils.h"
#include "graph/utils/type_utils_inner.h"
#include "graph/utils/tensor_adapter.h"
#include "api/gelib/gelib.h"
#include "graph/preprocess/insert_op/insert_aipp_op_util.h"
#include "api/aclgrph/option_utils.h"
#include "common/python_runtime/ge_python_runtime_manager.h"
#include "mmpa/mmpa_api.h"
#include "common/single_op_parser.h"
#include "parser/common/op_registration_tbe.h"
#include "framework/common/helper/model_helper.h"
#include "graph/utils/op_type_utils.h"
#include "graph_metadef/graph/operator_factory_impl.h"
#include "nlohmann/json.hpp"
#include "graph_metadef/graph/utils/file_utils.h"
#include "register/optimization_option_registry.h"
#include "register/op_lib_register_impl.h"
#include "register/amct_registry.h"

namespace {
using json = nlohmann::json;
using amctStatus = int32_t;
static bool is_dynamic_input = false;
const char *const kAmctSo = "libamctacl.so";
const char *const kLegacySoSuffix = "_legacy.so";
const char *const kModeSupport =
    "The value must be selected from the following: 0(model to framework model), "
    "1(framework model to json), 3(only pre-check), "
    "5(pbtxt to json), 6(display model info), "
    "7(convert a model to the OM2 format), "
    "30(model to execute-om for nano, an .om file for nano chips).";
const char *const kModelToJsonSupport =
    "The framework must be selected from {0(Caffe), 3(TensorFlow), 5(Onnx)} when model is set to 1(JSON).";
const char *const kCaffeFormatSupport = "The value must be NCHW or ND in Caffe model.";
const char *const kCaffeSupport = "Caffe is not supported in the current soc version";
const char *const kTFFormatSupport = "The value must be NCHW, NHWC, ND, NCDHW or NDHWC in TF model.";
const char *const kONNXFormatSupport = "The value must be NCHW, ND or NCDHW in ONNX model.";
const char *const kOptionBuildConfig = "ge.buildConfig";
// limit available mem size 2G
const long kMinAvailableMem = 2097152;  // 2 * 1024 * 1024

const std::string kFilePreffix(".om");
const std::string kPreloadFilePreffix(".exeom");
const std::string kOm2FilePreffix(".om2");
const std::string dbgSuffix(".dbg");

const std::map<ge::RunMode, std::string> kFilePrefixMap = {
    {ge::GEN_EXE_OM_FOR_NANO, kPreloadFilePreffix},
    {ge::GEN_OM2_MODEL, kOm2FilePreffix},
};

const int64_t kEnableFlag = 1;
const int32_t kBaseOfIntergerValue = 10;
const std::string kOffline = "offline";
void SetSingleCompileThread(std::map<std::string, std::string> &options) {
  const char_t *compile_thread_env = nullptr;
  MM_SYS_GET_ENV(MM_ENV_MULTI_THREAD_COMPILE, compile_thread_env);
  int64_t compile_thread =
      (compile_thread_env != nullptr) ? std::strtol(compile_thread_env, nullptr, kBaseOfIntergerValue) : kEnableFlag;
  options.emplace("MULTI_THREAD_COMPILE", std::to_string(compile_thread));
}
struct SrcModelAbstract {
  std::string model_file;
  std::string model_name;
  bool is_input_related;
  std::map<uint32_t, uint32_t> input_related_indices;
};
void SetBuildGraphModeOffline(std::map<std::string, std::string> &options) {
  GELOGI("build graph mode option set value to offset");
  options[ge::OPTION_BUILD_GRAPH_MODE] = kOffline;
}

void SetRegOptionNameMap(json &option_name_map) {
  const auto &reg_opt_tables = ge::OptionRegistry::GetInstance().GetVisibleOptions(ge::OoEntryPoint::kAtc);
  for (const auto &reg_opt : reg_opt_tables) {
    const auto readable_name = "--" + reg_opt.first;
    option_name_map.emplace(reg_opt.second.name, readable_name);
    GELOGD("The atc parameter [%s] is registered for the ir parameter [%s]", reg_opt.second.name.c_str(),
           readable_name.c_str());
  }
}

void SetOptionNameMap(std::map<std::string, std::string> &options) {
  json option_name_map;
  const auto cli_name_map = ge::BuildAtcGeOptionToCliNameMap();
  for (const auto &option_name : cli_name_map) {
    option_name_map.emplace(option_name.first, option_name.second);
  }
  SetRegOptionNameMap(option_name_map);
  options.emplace(ge::OPTION_NAME_MAP, option_name_map.dump());
}
}  // namespace

const std::unordered_set<std::string> kOm2UnsuppotedFlag = {
    "input_hint_shape",
    "om",
    "singleop",
    "check_report",
    "json",
    "virtual_type",
    "op_name_map",
    "quant_dumpable",
    "ac_parallel_enable",
    "tiling_schedule_optimize",
    "dump_mode",
    "display_model_info",
    "status_check",
    "save_original_model",
    "compress_weight_conf",
    "enable_compress_weight",
    "enable_attr_compression",
};

namespace ge {
const std::string kAtcUsageCommand = "atc";
const std::string kPyatcUsageCommand = "pyatc";
const std::string kPyatcModuleUsageCommand = "python3 -m ge.pyatc";
const std::string kPyatcModuleCommandSuffix = " -m ge.pyatc";
std::string g_usage_command = kAtcUsageCommand;

std::string GetFileName(const std::string &path) {
  const auto pos = path.find_last_of("/\\");
  return (pos == std::string::npos) ? path : path.substr(pos + 1U);
}

bool IsPyatcBinaryCommand(const std::string &argv0) {
  return GetFileName(argv0) == kPyatcUsageCommand;
}

bool IsPyatcModuleCommand(const std::string &argv0) {
  if (argv0.size() < kPyatcModuleCommandSuffix.size()) {
    return false;
  }
  return argv0.compare(argv0.size() - kPyatcModuleCommandSuffix.size(), kPyatcModuleCommandSuffix.size(),
                       kPyatcModuleCommandSuffix) == 0;
}

std::string ResolveUsageCommand(int32_t argc, char *argv[]) {
  if ((argc <= 0) || (argv == nullptr) || (argv[0] == nullptr)) {
    return kAtcUsageCommand;
  }
  const std::string argv0(argv[0]);
  if (IsPyatcBinaryCommand(argv0)) {
    return kPyatcUsageCommand;
  }
  if (IsPyatcModuleCommand(argv0)) {
    return kPyatcModuleUsageCommand;
  }
  return kAtcUsageCommand;
}

const char *const kRawCompileOptions = "compile options";
const char *const kRawCompileOptionsGlobal = "global";
const char *const kRawCompileOptionsSession = "session";
const char *const kRawCompileOptionsGraph = "graph";
const char *const kCliOptionPrefix = "--";
const std::set<std::string> kRawNonReplaceableCliOptions = {"model", "output", "framework", "soc_version"};

std::set<std::string> &GetRawAppliedFlagNames() {
  static std::set<std::string> raw_applied_flag_names;
  return raw_applied_flag_names;
}

std::map<std::string, std::string> &GetRawAppliedFlagOptions() {
  static std::map<std::string, std::string> raw_applied_flag_options;
  return raw_applied_flag_options;
}

std::string StripCliOptionPrefix(const std::string &cli_name) {
  if (cli_name.compare(0U, strlen(kCliOptionPrefix), kCliOptionPrefix) == 0) {
    return cli_name.substr(strlen(kCliOptionPrefix));
  }
  return cli_name;
}

bool IsRawNonReplaceableCliOption(const std::string &flag_name) {
  return kRawNonReplaceableCliOptions.find(flag_name) != kRawNonReplaceableCliOptions.end();
}

std::map<std::string, std::string> BuildRawGeOptionToCliNameMap() {
  auto option_to_cli_name = BuildAtcGeOptionToCliNameMap();
  const auto &visible_opt_table = OptionRegistry::GetInstance().GetVisibleOptions(OoEntryPoint::kAtc);
  for (const auto &visible_opt : visible_opt_table) {
    option_to_cli_name[visible_opt.second.name] = kCliOptionPrefix + visible_opt.first;
  }
  return option_to_cli_name;
}

bool IsCliOptionExplicitlySet(const std::string &ge_option,
                              const std::map<std::string, std::string> &option_to_cli_name,
                              const std::unordered_map<std::string, std::string> &user_options) {
  const auto cli_name_iter = option_to_cli_name.find(ge_option);
  if (cli_name_iter == option_to_cli_name.end()) {
    return false;
  }
  return user_options.find(StripCliOptionPrefix(cli_name_iter->second)) != user_options.end();
}

std::string GetRawCliName(const std::string &ge_option, const std::map<std::string, std::string> &option_to_cli_name) {
  const auto cli_name_iter = option_to_cli_name.find(ge_option);
  if (cli_name_iter == option_to_cli_name.end()) {
    return "";
  }
  return cli_name_iter->second;
}

Status ReadRawGeOptionsFile(const std::string &file_path, nlohmann::json &raw_json) {
  std::ifstream raw_file(file_path);
  if (!raw_file.is_open()) {
    GELOGE(FAILED, "[Check][RawGeOptions]open raw_ge_options file failed. file: %s", file_path.c_str());
    return FAILED;
  }

  raw_json = nlohmann::json::parse(raw_file, nullptr, false);
  if (raw_json.is_discarded()) {
    GELOGE(FAILED, "[Check][RawGeOptions]parse raw_ge_options json failed. file: %s", file_path.c_str());
    return FAILED;
  }
  if (!raw_json.is_object()) {
    GELOGE(FAILED, "[Check][RawGeOptions]raw_ge_options json must be an object. file: %s", file_path.c_str());
    return FAILED;
  }
  return SUCCESS;
}

Status ParseRawCompileOptionLevel(const nlohmann::json &compile_options, const std::string &level,
                                  std::map<std::string, std::string> &raw_options,
                                  std::map<std::string, std::string> &raw_option_levels) {
  const auto level_iter = compile_options.find(level);
  if (level_iter == compile_options.end()) {
    return SUCCESS;
  }
  if (!level_iter->is_object()) {
    GELOGE(FAILED, "[Check][RawGeOptions]compile options level [%s] must be an object.", level.c_str());
    return FAILED;
  }

  for (auto option_iter = level_iter->begin(); option_iter != level_iter->end(); ++option_iter) {
    if (option_iter.key().empty()) {
      GELOGE(FAILED, "[Check][RawGeOptions]option key in level [%s] must not be empty.", level.c_str());
      return FAILED;
    }
    if (!option_iter.value().is_string()) {
      GELOGE(FAILED, "[Check][RawGeOptions]option [%s] in level [%s] must be string.", option_iter.key().c_str(),
             level.c_str());
      return FAILED;
    }
    raw_options[option_iter.key()] = option_iter.value().get<std::string>();
    raw_option_levels[option_iter.key()] = level;
  }
  return SUCCESS;
}

Status ParseRawCompileOptions(const nlohmann::json &raw_json, std::map<std::string, std::string> &raw_options,
                              std::map<std::string, std::string> &raw_option_levels) {
  const auto compile_options_iter = raw_json.find(kRawCompileOptions);
  if ((compile_options_iter == raw_json.end()) || (!compile_options_iter->is_object())) {
    GELOGE(FAILED, "[Check][RawGeOptions]raw_ge_options must contain object [compile options].");
    return FAILED;
  }

  const std::set<std::string> supported_levels = {kRawCompileOptionsGlobal, kRawCompileOptionsSession,
                                                  kRawCompileOptionsGraph};
  for (auto level_iter = compile_options_iter->begin(); level_iter != compile_options_iter->end(); ++level_iter) {
    if (supported_levels.find(level_iter.key()) == supported_levels.end()) {
      GELOGE(FAILED, "[Check][RawGeOptions]unsupported compile options level [%s].", level_iter.key().c_str());
      return FAILED;
    }
  }

  // 根据优先级倒序解析option
  GE_ASSERT_SUCCESS(
      ParseRawCompileOptionLevel(*compile_options_iter, kRawCompileOptionsGlobal, raw_options, raw_option_levels),
      "[Parse][RawCompileOptionLevel]global failed.");
  GE_ASSERT_SUCCESS(
      ParseRawCompileOptionLevel(*compile_options_iter, kRawCompileOptionsSession, raw_options, raw_option_levels),
      "[Parse][RawCompileOptionLevel]session failed.");
  GE_ASSERT_SUCCESS(
      ParseRawCompileOptionLevel(*compile_options_iter, kRawCompileOptionsGraph, raw_options, raw_option_levels),
      "[Parse][RawCompileOptionLevel]graph failed.");
  return SUCCESS;
}

Status CheckRawRegisteredOptimizationOptionValue(const std::string &option_name, const std::string &option_value) {
  const auto &visible_opt_table = OptionRegistry::GetInstance().GetVisibleOptions(OoEntryPoint::kAtc);
  for (const auto &visible_opt : visible_opt_table) {
    if (visible_opt.second.name != option_name) {
      continue;
    }
    if (visible_opt.second.checker == nullptr) {
      return SUCCESS;
    }
    std::string reason = "Invalid optimization option value.";
    if (visible_opt.second.checker(option_value, reason)) {
      return SUCCESS;
    }
    GELOGE(PARAM_INVALID, "[Check][RawGeOptions]option [%s] value [%s] is invalid. %s", option_name.c_str(),
           option_value.c_str(), reason.c_str());
    return PARAM_INVALID;
  }
  return SUCCESS;
}

Status ValidateRawGeOptionByCliParser(const std::string &option_name, const std::string &option_value,
                                      const std::string &cli_name) {
  const std::string flag_name = StripCliOptionPrefix(cli_name);
  const flgs::GfStatus ret = flgs::CheckFlagValue(flag_name, option_value);
  if (ret == flgs::GF_FAILED) {
    GELOGE(FAILED, "[Check][RawGeOptions]option [%s] value [%s] is invalid by cli option [%s].", option_name.c_str(),
           option_value.c_str(), cli_name.c_str());
    return FAILED;
  }
  return SUCCESS;
}

Status ValidateRawGeOptionValue(const std::string &option_name, const std::string &option_value,
                                const std::string &cli_name) {
  GE_ASSERT_SUCCESS(ValidateRawGeOptionByCliParser(option_name, option_value, cli_name),
                    "[Check][RawGeOptionByCliParser]failed.");

  if (option_name == OPTION_STATIC_MODEL_OPS_LOWER_LIMIT) {
    GE_ASSERT_SUCCESS(CheckValidValueRange(cli_name, option_value, -1L, std::numeric_limits<int64_t>::max()),
                      "[Check][RawGeOptions]static_model_ops_lower_limit failed.");
  }
  GE_ASSERT_SUCCESS(CheckRawRegisteredOptimizationOptionValue(option_name, option_value),
                    "[Check][RawGeOptions][OptimizationOption]failed.");
  return SUCCESS;
}

Status ApplyRawGeOptionsToFlags(const std::map<std::string, std::string> &raw_options) {
  const auto option_to_cli_name = BuildRawGeOptionToCliNameMap();
  const auto &user_options = flgs::GetUserOptions();
  for (const auto &raw_option : raw_options) {
    if (IsCliOptionExplicitlySet(raw_option.first, option_to_cli_name, user_options)) {
      continue;
    }
    const std::string cli_name = GetRawCliName(raw_option.first, option_to_cli_name);
    if (cli_name.empty()) {
      continue;
    }
    const std::string flag_name = StripCliOptionPrefix(cli_name);
    if (IsRawNonReplaceableCliOption(flag_name)) {
      const std::string reason =
          "This option must be set explicitly by command line and cannot be replaced by "
          "raw_ge_options.";
      REPORT_PREDEFINED_ERR_MSG(
          "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
          std::vector<const char *>({cli_name.c_str(), raw_option.second.c_str(), reason.c_str()}));
      GELOGE(FAILED,
             "[Check][RawGeOptions]raw option [%s] maps to required cli option [%s], "
             "which must be set explicitly by command line.",
             raw_option.first.c_str(), cli_name.c_str());
      return FAILED;
    }
    if (flgs::SetFlagValue(flag_name, raw_option.second) != flgs::GF_SUCCESS) {
      GELOGE(FAILED, "[Set][RawGeOptions]set raw option [%s] value [%s] to cli option [%s] failed.",
             raw_option.first.c_str(), raw_option.second.c_str(), cli_name.c_str());
      return FAILED;
    }
    (void)GetRawAppliedFlagNames().emplace(flag_name);
    GetRawAppliedFlagOptions()[flag_name] = raw_option.second;
    GELOGI("[Set][RawGeOptions]set cli option [%s] by raw option [%s].", cli_name.c_str(), raw_option.first.c_str());
  }
  return SUCCESS;
}

Status FilterAndValidateRawGeOptions(std::map<std::string, std::string> &raw_options,
                                     std::map<std::string, std::string> &raw_option_levels) {
  const auto option_to_cli_name = BuildRawGeOptionToCliNameMap();
  for (auto option_iter = raw_options.begin(); option_iter != raw_options.end();) {
    const auto support_iter = option_to_cli_name.find(option_iter->first);
    if (support_iter == option_to_cli_name.end()) {
      const auto level_iter = raw_option_levels.find(option_iter->first);
      const std::string level = (level_iter == raw_option_levels.end()) ? "" : level_iter->second;
      if (!FLAGS_raw_ge_options_ignore_unsupported) {
        const std::string reason = "RawGeOptions[" + option_iter->first + ":" + option_iter->second + "] in level [" +
                                   level + "] is not supported in ATC. Please do not set it.";
        REPORT_PREDEFINED_ERR_MSG("E10055", std::vector({"reason"}), std::vector({reason.c_str()}));
        GELOGE(FAILED, "[Check][RawGeOptions]option [%s] in level [%s] is not supported by atc. value: %s",
               option_iter->first.c_str(), level.c_str(), option_iter->second.c_str());
        return FAILED;
      }
      GELOGW("[Check][RawGeOptions]ignore unsupported option [%s] in level [%s]. value: %s", option_iter->first.c_str(),
             level.c_str(), option_iter->second.c_str());
      (void)raw_option_levels.erase(option_iter->first);
      option_iter = raw_options.erase(option_iter);
      continue;
    }

    GE_ASSERT_SUCCESS(ValidateRawGeOptionValue(option_iter->first, option_iter->second, support_iter->second),
                      "[Check][RawGeOptionValue]failed.");
    ++option_iter;
  }
  return SUCCESS;
}

Status LoadRawGeOptions(std::map<std::string, std::string> &raw_options) {
  if (FLAGS_raw_ge_options.empty()) {
    return SUCCESS;
  }
  GE_CHK_BOOL_EXEC(CheckInputPathValid(FLAGS_raw_ge_options, "--raw_ge_options"), return FAILED,
                   "[Check][RawGeOptions] file %s not found.", FLAGS_raw_ge_options.c_str());

  nlohmann::json raw_json;
  std::map<std::string, std::string> raw_option_levels;
  // parser json
  GE_ASSERT_SUCCESS(ReadRawGeOptionsFile(FLAGS_raw_ge_options, raw_json), "[Read][RawGeOptionsFile]failed.");
  // 根据优先级倒序解析option
  GE_ASSERT_SUCCESS(ParseRawCompileOptions(raw_json, raw_options, raw_option_levels),
                    "[Parse][RawCompileOptions]failed.");
  return FilterAndValidateRawGeOptions(raw_options, raw_option_levels);
}

void MergeRawGeOptions(const std::map<std::string, std::string> &raw_options,
                       std::map<std::string, std::string> &options) {
  const auto atc_option_to_cli_name = BuildAtcGeOptionToCliNameMap();
  const auto option_to_cli_name = BuildRawGeOptionToCliNameMap();
  const auto &user_options = flgs::GetUserOptions();
  for (const auto &raw_option : raw_options) {
    if (atc_option_to_cli_name.find(raw_option.first) != atc_option_to_cli_name.end()) {
      GELOGI("[Merge][RawGeOptions]skip option [%s] because it is already applied through atc cli path.",
             raw_option.first.c_str());
      continue;
    }
    if (IsCliOptionExplicitlySet(raw_option.first, option_to_cli_name, user_options)) {
      const auto cli_iter = option_to_cli_name.find(raw_option.first);
      GELOGI("[Merge][RawGeOptions]skip option [%s] because cli option [%s] is explicitly set.",
             raw_option.first.c_str(), cli_iter->second.c_str());
      continue;
    }
    GELOGI("[Merge][RawGeOptions]set option [%s] by raw_ge_options.", raw_option.first.c_str());
    options[raw_option.first] = raw_option.second;
  }
}

class GFlagUtils {
 public:
  /**
   * @name   InitGFlag
   * @brief  initialize gflag
   * @return void
   */
  static flgs::GfStatus InitGFlag(int32_t argc, char *argv[]) {
    // -help
    std::stringstream os_help_info;
    std::stringstream cpu_help_info;
    GenHostEnvOsAndCpuHelpInfo(os_help_info, cpu_help_info);
    std::vector<std::string> oo_help_info;
    oo_help_info.resize(static_cast<size_t>(OoCategory::kEnd));
    const auto ret = GenAndRegOptimizationOptions(oo_help_info);
    if (ret == flgs::GF_FAILED) {
      return ret;
    }
    g_usage_command = ResolveUsageCommand(argc, argv);
    flgs::SetUsageMessage(
        "usage: " + g_usage_command +
        " <args>\n"
        "generate offline model example:\n" +
        g_usage_command +
        " --model=./alexnet.prototxt --weight=./alexnet.caffemodel --framework=0 --output=./domi "
        "--soc_version=<soc_version> \n"
        "generate offline model for single op example:\n" +
        g_usage_command +
        " --singleop=./op_list.json --output=./op_model --soc_version=<soc_version> \n"
        "\n===== Basic Functionality =====\n"
        "[General]\n"
        "  --h/help            Show this help message\n"
        "  --mode              Run mode.\n"
        "                       0: default, generate offline model;\n"
        "                       1: convert model to JSON format;\n"
        "                       3: only pre-check;\n"
        "                       5: convert ge dump txt file to JSON format;\n"
        "                       6: display model info;\n"
        "                       30: convert original graph to execute-om for nano(offline model)\n"
        "  --raw_ge_options    Raw GE options json file path. Only \"compile options\" will be parsed.\n"
        "  --raw_ge_options_ignore_unsupported    Ignore unsupported raw GE options. false(default): report error; "
        "true: ignore unsupported options.\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kGeneral)] +
        "\n[Input]\n"
        "  --model             Model file\n"
        "  --weight            Weight file. Required when framework is Caffe\n"
        "  --om                The model file to be converted to json\n"
        "  --framework         Framework type. 0:Caffe; 1:MindSpore; 3:Tensorflow; 5:Onnx\n"
        "  --input_format      Format of input data. E.g.: \"NCHW\"\n"
        "  --input_shape       Shape of static input data or shape range of dynamic input. "
        "Separate multiple nodes with semicolons (;). "
        "Use double quotation marks (\") to enclose each argument.\n"
        "                      E.g.: \"input_name1:n1,c1,h1,w1;input_name2:n2,c2,h2,w2\"\n"
        "                            \"input_name1:n1~n2,c1,h1,w1;input_name2:n3~n4,c2,h2,w2\"\n"
        "  --input_hint_shape  Shape hint of dynamic input. "
        "Separate multiple nodes with semicolons (;). "
        "Use double quotation marks (\") to enclose each argument.\n"
        "                      E.g.: \"index1:[n1,c1,h1,w1];index2:[n2,c2,h2,w2]\"\n"
        "  --input_shape_range "
        "This option is deprecated and will be removed in future version, please use input_shape instead."
        "Shape range of input data. Separate multiple nodes with semicolons (;).\n"
        "                      Use double quotation marks (\") to enclose each argument.\n"
        "                      E.g.: \"input_name1:[n1~n2,c1,h1,w1];input_name2:[n2,c2~c3,h2,w2]\"\n"
        "  --dynamic_batch_size Set dynamic batch size. E.g.: \"batchsize1,batchsize2,batchsize3\"\n"
        "  --dynamic_image_size Set dynamic image size. Separate multiple nodes with semicolons (;). "
        "Use double quotation marks (\") to enclose each argument.\n"
        "                       E.g.: \"imagesize1_height,imagesize1_width;imagesize2_height,imagesize2_width\"\n"
        "  --dynamic_dims      Set dynamic dims. Separate multiple nodes with semicolons (;). "
        "Use double quotation marks (\") to enclose each argument.\n"
        "                       E.g.: \"dims1_n1,dims1_n2;dims2_n1,dims2_n2\"\n"
        "  --singleop          Single op definition file. atc will generate offline."
        "model(s) for single op if --singleop is set.\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kInput)] +
        "\n[Output]\n"
        "  --output            Output file path&name(needn't suffix, will add .om/.exeom automatically).\n"
        "                      If --mode is set to 30, an additional dbg file will be generated.\n"
        "                      If --singleop is set, this arg specifies the directory to "
        "which the single op offline model will be generated.\n"
        "  --output_type       Set net output type. Support FP32, FP16, UINT8, INT8. "
        "E.g.: FP16, indicates that all out nodes are set to FP16.\n"
        "                      \"node1:0:FP16;node2:1:FP32\", indicates setting the datatype of multiple out nodes.\n"
        "  --check_report      The pre-checking report file. Default value is: \"check_result.json\"\n"
        "  --json              The output json file path&name which is converted from a model\n" +
        os_help_info.str() + cpu_help_info.str() + oo_help_info[static_cast<size_t>(OoCategory::kOutput)] +
        "\n[Target]\n"
        "  --soc_version       The soc version.\n"
        "  --virtual_type      Set whether offline model can run on the virtual devices under compute "
        "capability allocation.\n"
        "                      0 (default) : Disable virtualization; 1 : Enable virtualization.\n"
        "  --core_type         Set core type AiCore or VectorCore. VectorCore: use vector core. "
        "Default value is: AiCore\n"
        "  --aicore_num        Set aicore num\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kTarget)] +
        "===== Advanced Functionality =====\n"
        "[Feature]\n"
        "  --out_nodes         Output nodes designated by users. Separate multiple nodes with semicolons (;)."
        "Use double quotation marks (\") to enclose each argument.\n"
        "                      E.g.: \"node_name1:0;node_name1:1;node_name2:0\"\n"
        "  --input_fp16_nodes  Input node datatype is fp16. Separate multiple nodes with semicolons (;). "
        "Use double quotation marks (\") to enclose each argument. "
        "E.g.: \"node_name1;node_name2\"\n"
        "  --insert_op_conf    Config file to insert new op\n"
        "  --op_name_map       Custom op name mapping file\n"
        "                      Note: A semicolon(;) cannot be included in each "
        "path, otherwise the resolved path will not match the expected one.\n"
        "  --is_input_adjust_hw_layout    Input node datatype is fp16 and format is NC1HWC0, used with input_fp16_nodes"
        ". true: enable; false(default): disable. E.g.: \"true,true,false,true\"\n"
        "  --is_output_adjust_hw_layout   Net output node datatype is fp16 and format is NC1HWC0, used with out_nodes. "
        "true: enable; false(default): disable. E.g.: \"true,true,false,true\"\n"
        "  --external_weight        Convert const to file constant, and save weight in file.\n"
        "                           0 (default): save weight in om.  1: save weight in file.  2: save all weights in "
        "one file.\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kFeature)] +
        "\n[Model Tuning]\n"
        "  --disable_reuse_memory    The switch of reuse memory. Default value is : 0. "
        "0 means reuse memory, 1 means do not reuse memory.\n"
        "  --fusion_switch_file      File for fusion rule(graph fusion and UB fusion).\n"
        "                            Enter as the configuration file path, disable specified fusion rules\n"
        "  --enable_scope_fusion_passes    validate the non-general scope fusion passes, "
        "multiple names can be set and separated by ','. E.g.: ScopePass1,ScopePass2,...\n"
        "  --enable_single_stream    Enable single stream. true: enable; false(default): disable\n"
        "  --ac_parallel_enable      Enable engines such as Aicpu to parallel with other engines in dynamic shape "
        "graphs. 1: enable; 0(default): disable\n"
        "  --tiling_schedule_optimize Enable tiling schedule optimize. 1: enable; 0(default): disable\n"
        "  --quant_dumpable          Ensure that the input and output of quant nodes can be dumped. "
        "1: enable; 0(default): disable.\n"
        "  --cluster_config          Set the path of cluster configuration file for target execute logic device info "
        "to generate hccl tasks.\n"
        "  --hccl_sub_comm_config  Set the path of HCCL sub-communication configuration file.\n"
        "                           Used to configure HCCL sub-communication parameters.\n"
        "  --enable_small_channel    Set enable small channel. 0(default): disable; 1: enable\n"
        "  --enable_compress_weight  Enable compress weight. true: enable; false(default): disable\n"
        "  --compress_weight_conf    Config file to compress weight\n"
        "  --compression_optimize_conf    Config file to compress optimize\n"
        "  --enable_attr_compression  Enable attribute compression. true(default): enable; false: disable\n"
        "  --sparsity                Optional; enable structured sparse. 0(default): disable; 1: enable\n"
        "  --buffer_optimize         Set buffer optimize. Support \"l2_optimize\" (default), "
        "\"l1_optimize\", \"off_optimize\"\n"
        "  --mdl_bank_path           Set the path of the custom repository generated after model tuning.\n" +
        "  --oo_level                The graph optimization level. Support \"O1\", \"O3\"(default).\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kModelTuning)] +
        "\n[Operator Tuning]\n"
        "  --op_precision_mode     Set the path of operator precision mode configuration file (.ini)\n"
        "  --allow_hf32            enable hf32. false: disable; true: enable. (not support, reserved)\n"
        "  --precision_mode        precision mode, support force_fp16(default), force_fp32, cube_fp16in_fp32out, "
        "allow_mix_precision, allow_fp32_to_fp16, must_keep_origin_dtype, allow_mix_precision_fp16, "
        "allow_mix_precision_bf16, allow_fp32_to_bf16.\n"
        "  --precision_mode_v2     precision mode v2, support fp16(default), origin, cube_fp16in_fp32out, "
        "mixed_float16, "
        "mixed_bfloat16, cube_hif8, mixed_hif8.\n"
        "  --modify_mixlist        Set the path of operator mixed precision configuration file.\n"
        "  --keep_dtype            Retains the precision of certain operators in inference "
        "scenarios by using a configuration file.\n"
        "  --customize_dtypes      Set the path of custom dtypes configuration file.\n"
        "  --is_weight_clip        Ensure weight is finite by clipped when its datatype is floating-point data, "
        "0: disable; 1(default): enable.\n"
        "  --op_bank_path          Set the path of the custom repository generated after operator tuning with Auto "
        "Tune.\n"
        "  --jit_compile          Set jit compile mode. 0: use binary first; 1(default): compile online; "
        "2: compile online for static shape and use binary for dynamic shape.\n"
        "  --optimization_switch  Set graph optimization pass switch. "
        "E.g.: \"pass_name1:on;pass_name2:off\"\n"
        "  --static_model_ops_lower_limit    Set the lower limit of static subgraph op count in dynamic shape "
        "partition. The value must be an integer greater than or equal to -1.\n"
        "  --op_select_implmode    Set op select implmode. Support high_precision, high_performance, "
        "high_precision_for_all, high_performance_for_all. default: high_performance\n"
        "  --optypelist_for_implmode    Appoint which op to select implmode, cooperated with op_select_implmode.\n"
        "                               Separate multiple nodes with commas (,). Use double quotation marks (\") "
        "to enclose each argument. E.g.: \"node_name1,node_name2\"\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kOperatorTuning)] +
        "\n[Debug]\n"
        "  --op_debug_level        Debug enable for TBE operator building.\n"
        "                          0 (default): Disable debug; 1: Enable TBE pipe_all, "
        "and generate the operator CCE file and Python-CCE mapping file (.json);\n"
        "                          2: Enable TBE pipe_all, generate the operator CCE file and Python-CCE mapping file "
        "(.json), and enable the CCE compiler -O0-g.\n"
        "                          3: Disable debug, and keep generating kernel file (.o and .json)\n"
        "                          4: Disable debug, keep generation kernel file (.o and .json) and generate the "
        "operator CCE file (.cce) and the UB fusion computing description file (.json)\n"
        "  --save_original_model   Control whether to output original model. E.g.: true: output original model\n"
        "  --log                   Generate log with level. Support debug, info, warning, error, null(default)\n"
        "  --dump_mode             The switch of dump json with shape, to be used with mode 1. "
        "0(default): disable; 1: enable.\n"
        "  --debug_dir             Set the save path of operator compilation intermediate files.\n"
        "                          Default value: ./kernel_meta\n"
        "  --status_check             switch for op status check such as overflow.\n"
        "                             0(default): disable; 1: enable.\n"
        "  --op_compiler_cache_dir    Set the save path of operator compilation cache files.\n"
        "                             Default value: $HOME/atc_data\n"
        "  --op_compiler_cache_mode   Set the operator compilation cache mode. "
        "Options are disable(default), enable and force(force to refresh the cache)\n"
        "  --display_model_info     enable for display model info; 0(default): close display, 1: open display.\n"
        "  --build_config           Make command template for generated source compilation. "
        "GE adds the generated Makefile path automatically. "
        "Allowed variables: CXX, CXXFLAGS, CPPFLAGS, LDFLAGS, LDLIBS. "
        "E.g.: \"make -s -j16 CXX=/usr/bin/clang++ CXXFLAGS='-O2'\"\n"
        "  --shape_generalized_build_mode    For selecting the mode of shape generalization when build graph.\n"
        "                                    shape_generalized: Shape will be generalized during graph build\n"
        "                                    shape_precise(default): Shape will not be generalized, use precise shape\n"
        "  --op_debug_config        Debug enable for Operator memory detection, enter as the configuration file path.\n"
        "                           If option is default, debug for Operator memory detection is disable. \n"
        "  --atomic_clean_policy    For selecting the atomic op clean memory policy.\n"
        "                           0 (default): centralized clean.  1: separate clean.\n"
        "  --deterministic          For deterministic calculation.\n"
        "                           0 (default): deterministic off. 1: deterministic on.\n"
        "  --deterministic_level    For deterministic and strong consistency calculation.\n"
        "                           0 (default): deterministic off. 1: deterministic on. 2: strong consistency on.\n" +
        oo_help_info[static_cast<size_t>(OoCategory::kDebug)]);

    return flgs::ParseCommandLine(argc, argv);
  }

  static void GenHostEnvOsAndCpuHelpInfo(std::stringstream &host_env_os_info, std::stringstream &host_env_cpu_info) {
    std::unordered_map<std::string, std::unordered_set<std::string>> opp_supported_os_cpu;
    std::string default_os;
    std::string default_cpu;
    PluginManager::GetOppSupportedOsAndCpuType(opp_supported_os_cpu);
    PluginManager::GetCurEnvPackageOsAndCpuType(default_os, default_cpu);
    host_env_os_info
        << "  --host_env_os            OS type of the target execution environment.\n"
           "                           The parameters that support setting are the OS types of the opp package\n"
           "                           Supported host env os as list:\n";
    host_env_os_info << "                           ";
    for (const auto &it : opp_supported_os_cpu) {
      host_env_os_info << it.first << " ";
    }
    host_env_os_info << "\n";
    host_env_os_info << "                           default: " << default_os << "\n";

    host_env_cpu_info
        << "  --host_env_cpu           CPU type of the target execution environment.\n"
           "                           The parameters that support setting are the CPU types of the opp package\n"
           "                           Supported host env cpu as list:\n";
    for (const auto &it0 : opp_supported_os_cpu) {
      host_env_cpu_info << "                           support cpu: ";
      for (const auto &it1 : opp_supported_os_cpu[it0.first]) {
        host_env_cpu_info << it1 << " ";
      }
      host_env_cpu_info << ", respond to os: " << it0.first << "\n";
    }
    host_env_cpu_info << "                           default: " << default_cpu << "\n";
    return;
  }

  static bool IsRequiredParameterExists(const char *const param_name, const std::string &param_value) {
    if (!param_value.empty()) {
      return true;
    }
    REPORT_PREDEFINED_ERR_MSG("E10054", std::vector<const char *>({"parameter"}),
                              std::vector<const char *>({param_name}));
    GELOGE(FAILED,
           "The required parameter [%s] for ATC is empty. "
           "Another possible reason is that the value of some parameters is not enclosed by quotation marks (\"\").",
           param_name);
    return false;
  }

  static bool CheckOutputPathWithSuffix(const std::string &path, const std::string &atc_param) {
    std::string file_path(path);
    const auto it = kFilePrefixMap.find(static_cast<RunMode>(FLAGS_mode));
    if (it == kFilePrefixMap.end()) {
      file_path += kFilePreffix;
    } else {
      file_path += it->second;
    }
    return CheckOutputPathValid(file_path, atc_param);
  }

  static bool CheckDbgPathWithSuffix(const std::string &path, const std::string &atc_param) {
    const std::string file_path = path + dbgSuffix;
    return CheckOutputPathValid(file_path, atc_param);
  }

  static bool CheckWeightAndFrameWork() {
    if ((FLAGS_mode == static_cast<int32_t>(RunMode::MODEL_TO_JSON)) &&
        ((FLAGS_framework != domi::TENSORFLOW) && (FLAGS_framework != domi::CAFFE) && (FLAGS_framework != domi::ONNX) &&
         (FLAGS_framework != -1))) {
      REPORT_PREDEFINED_ERR_MSG(
          "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
          std::vector<const char *>({"--framework", std::to_string(FLAGS_framework).c_str(), kModelToJsonSupport}));
      DOMI_LOGE("[Convert][ModelToJson]Invalid value for --framework[%d], %s.", FLAGS_framework, kModelToJsonSupport);
      return false;
    }
    if ((!FLAGS_weight.empty()) && (!CheckInputPathValid(FLAGS_weight, "--weight"))) {
      DOMI_LOGE("[Check][Param:weight]value:%s: is invalid, path cannot reach.", FLAGS_weight.c_str());
      return false;
    }
    if ((FLAGS_mode != static_cast<int32_t>(RunMode::GEN_OM_MODEL)) &&
        (FLAGS_mode != static_cast<int32_t>(RunMode::GEN_EXE_OM_FOR_NANO)) &&
        (FLAGS_mode != static_cast<int32_t>(RunMode::ONLY_PRE_CHECK))) {
      return true;  // 其他情况不校验framework取值是否默认值-1
    }
    if (FLAGS_framework == -1) {
      const std::string support = "0(Caffe) or 1(MindSpore) or 3(TensorFlow) or 5(Onnx)";
      REPORT_PREDEFINED_ERR_MSG("E10007", std::vector<const char *>({"parameter", "support"}),
                                std::vector<const char *>({"framework", support.c_str()}));
      DOMI_LOGE("[Check][Parameter] When --mode=%d, the value of --framework must be[%s].", FLAGS_mode,
                support.c_str());
      return false;
    } else if (FLAGS_framework == static_cast<int32_t>(domi::CAFFE)) {
      // The Soc Version check for caffe model conversion has been removed, so errors may occur in later processes.
      if (FLAGS_weight.empty()) {
        REPORT_PREDEFINED_ERR_MSG("E10008", std::vector<const char *>({"parameter"}),
                                  std::vector<const char *>({"--weight"}));
        DOMI_LOGE("Input parameter[--weight]'s value is empty when framework is 0(CAFFE)!");
        return false;
      }
    } else {
      if (!FLAGS_weight.empty()) {
        if (FLAGS_framework == static_cast<int32_t>(domi::TENSORFLOW)) {
          GELOGW("Parameter weight is ignored for TensorFlow.");
        }
        if (FLAGS_framework == static_cast<int32_t>(domi::ONNX)) {
          GELOGW("Parameter weight is ignored for Onnx.");
        }
      }
    }
    return true;
  }

  static Status CheckOm2OptionValid(const std::string &name, const std::string &value) {
    GELOGI("start to check option[%s], value[%s]", name.c_str(), value.c_str());
    if (kOm2UnsuppotedFlag.find(name) == kOm2UnsuppotedFlag.end()) {
      return ge::SUCCESS;
    }
    REPORT_PREDEFINED_ERR_MSG(
        "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
        std::vector<const char *>({name.c_str(), value.c_str(), "this option is not supported in om2 mode."}));
    GELOGE(ge::PARAM_INVALID, "[Check][Option]option [%s] is not supported in om2 mode.", name.c_str());
    return ge::PARAM_INVALID;
  }

  static Status CheckOm2UserOptionsValid(const std::unordered_map<std::string, std::string> &user_options) {
    for (const auto &opt : user_options) {
      GE_ASSERT_SUCCESS(CheckOm2OptionValid(opt.first, opt.second), "[Check][OM2][UserOption] failed!");
    }
    for (const auto &opt : GetRawAppliedFlagOptions()) {
      GE_ASSERT_SUCCESS(CheckOm2OptionValid(opt.first, opt.second), "[Check][OM2][RawOption] failed!");
    }
    return ge::SUCCESS;
  }

  static Status CheckPrecisionRelatedFlags() {
    GE_ASSERT_SUCCESS(CheckIsWeightClipParamValid(FLAGS_is_weight_clip), "[Check][is_weight_clip]failed!");
    GE_ASSERT_SUCCESS(CheckPrecisionModeParamValid(FLAGS_precision_mode), "[Check][PrecisionMode]failed!");
    GE_ASSERT_SUCCESS(CheckPrecisionModeV2ParamValid(FLAGS_precision_mode_v2), "[Check][PrecisionModeV2]failed!");
    GE_ASSERT_SUCCESS(CheckPrecisionModeV2Conflict(FLAGS_precision_mode, FLAGS_precision_mode_v2),
                      "[Check][PrecisionModeV2Conflict]failed!");
    if (CheckModifyMixlistParamValid(FLAGS_precision_mode, FLAGS_precision_mode_v2, FLAGS_modify_mixlist) != SUCCESS) {
      return FAILED;
    }
    return SUCCESS;
  }

  static Status CheckFlags() {
    const bool is_mode_om = ((FLAGS_mode == static_cast<int32_t>(RunMode::GEN_OM_MODEL)) ||
                             (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_EXE_OM_FOR_NANO)) ||
                             (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_OM2_MODEL)));

    const bool is_dbg = (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_EXE_OM_FOR_NANO));

    /* Check the validity of the I / O file path */
    bool is_invalid_input = (!IsRequiredParameterExists("--soc_version", FLAGS_soc_version)) ||
                            (is_mode_om && (!IsRequiredParameterExists("--output", FLAGS_output)));
    if (is_invalid_input) {
      return FAILED;
    }
    GE_CHK_BOOL_RET_STATUS_NOLOG(IsRequiredParameterExists("--model", FLAGS_model), FAILED);
    GE_CHK_BOOL_RET_STATUS(CheckInputPathValid(FLAGS_model, "--model"), FAILED,
                           "[Check][InputPath]model file %s not found!!", FLAGS_model.c_str());

    is_invalid_input =
        is_mode_om && (!CheckOutputPathWithSuffix(FLAGS_output, "--output") || !CheckPathWithName(FLAGS_output));
    if (is_invalid_input) {
      GELOGE(FAILED, "[Check][OutputPath]output path is not valid!! path: [%s]", FLAGS_output.c_str());
      return FAILED;
    }

    bool is_invalid_dbg = is_dbg && (!CheckDbgPathWithSuffix(FLAGS_output, "--output"));
    if (is_invalid_dbg) {
      GELOGE(FAILED, "[Check][OutputPath] dbg path is not valid!! path: [%s]", FLAGS_output.c_str());
      return FAILED;
    }

    // check param disable_reuse_memory
    GE_CHK_BOOL_EXEC(CheckDisableReuseMemoryParamValid(std::to_string(FLAGS_disable_reuse_memory)) == SUCCESS,
                     return FAILED, "[Check][DisableReuseMemory]failed!");

    // check optypelist_for_implmode and op_select_implmode
    GE_CHK_BOOL_EXEC(CheckImplmodeParamValid(FLAGS_optypelist_for_implmode, FLAGS_op_select_implmode) == SUCCESS,
                     return FAILED, "[Check][ImplMode]check optypelist_for_implmode and op_select_implmode failed!");

    is_invalid_input =
        (!FLAGS_op_precision_mode.empty()) && (!CheckInputPathValid(FLAGS_op_precision_mode, "--op_precision_mode"));
    if (is_invalid_input) {
      REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                                std::vector<const char *>({"--op_precision_mode", FLAGS_op_precision_mode.c_str(),
                                                           "Path defined by op precision mode is not found."}));
      GELOGE(FAILED, "[Check][op_precision_mode] %s not found", FLAGS_op_precision_mode.c_str());
      return FAILED;
    }
    GE_ASSERT_SUCCESS(CheckPrecisionRelatedFlags());

    if (CheckAndTransferInputShapeToRange(FLAGS_input_shape, FLAGS_input_shape_range, FLAGS_dynamic_batch_size,
                                          FLAGS_dynamic_image_size, FLAGS_dynamic_dims) != SUCCESS) {
      GELOGE(FAILED, "[Check][TransferShapeAndRange] Transfer shape to shape range failed!");
      return FAILED;
    }
    GE_ASSERT_SUCCESS(CheckHintShapeConflictWithDynamicParam(FLAGS_input_hint_shape, FLAGS_dynamic_batch_size,
                                                             FLAGS_dynamic_image_size, FLAGS_dynamic_dims),
                      "[Check][input hint shape] failed!");
    if (CheckDynamicInputParamValid(FLAGS_dynamic_batch_size, FLAGS_dynamic_image_size, FLAGS_dynamic_dims,
                                    FLAGS_input_shape, FLAGS_input_shape_range, FLAGS_input_format,
                                    is_dynamic_input) != SUCCESS) {
      GELOGE(FAILED, "[Check][DynamicInput]dynamic size(batch size, image size or dims) invalid!");
      return FAILED;
    }

    is_invalid_input = !FLAGS_insert_op_conf.empty() && !FLAGS_dynamic_dims.empty();
    if (is_invalid_input) {
      REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                                std::vector<const char *>({"--insert_op_conf", FLAGS_insert_op_conf.c_str(),
                                                           "The dynamic dims function does not support AIPP."}));
      GELOGE(FAILED, "[Check][Param]dynamic dims function does not support aipp");
      return FAILED;
    }

    is_invalid_input = !FLAGS_weight.empty() && !CheckInputPathValid(FLAGS_weight, "--weight");
    if (is_invalid_input) {
      GELOGE(FAILED, "[Check][InputPath]weight file %s not found!!", FLAGS_weight.c_str());
      return FAILED;
    }

    is_invalid_input = !FLAGS_cal_conf.empty() && !CheckInputPathValid(FLAGS_cal_conf, "--cal_conf");
    if (is_invalid_input) {
      GELOGE(FAILED, "[Check][InputPath]calibration config file %s not found!!", FLAGS_cal_conf.c_str());
      return FAILED;
    }

    is_invalid_input = !FLAGS_op_name_map.empty() && !CheckInputPathValid(FLAGS_op_name_map, "--op_name_map");
    if (is_invalid_input) {
      GELOGE(FAILED, "[Check][InputPath]op config file %s not found!!", FLAGS_op_name_map.c_str());
      return FAILED;
    }

    GE_CHK_BOOL_EXEC(CheckInsertOpConfParamValid(std::string(FLAGS_insert_op_conf)) == SUCCESS, return FAILED,
                     "[Check][InsertOpConf]failed!");

    GE_CHK_BOOL_EXEC(CheckCompressWeightParamValid(FLAGS_enable_compress_weight, FLAGS_compress_weight_conf) == SUCCESS,
                     return FAILED, "[Check][CompressWeight]failed!");

    GE_CHK_BOOL_EXEC(CheckKeepTypeParamValid(FLAGS_keep_dtype) == SUCCESS, return FAILED, "[Check][KeepType]failed!");

    GE_CHK_BOOL_EXEC(CheckOutputPathValid(FLAGS_check_report, "--check_report"), return FAILED,
                     "[Check][OutputPath]]check_report file %s not found!!", FLAGS_check_report.c_str());

    is_invalid_input = (!FLAGS_save_original_model.empty()) && (FLAGS_save_original_model != "true") &&
                       (FLAGS_save_original_model != "false");
    if (is_invalid_input) {
      REPORT_PREDEFINED_ERR_MSG("E10005", std::vector<const char *>({"parameter", "value"}),
                                std::vector<const char *>({"save_original_model", FLAGS_save_original_model.c_str()}));
      GELOGE(FAILED, "[Check][Parameter]Input parameter[--save_original_model]'s value[%s] must be true or false.",
             FLAGS_save_original_model.c_str());
      return FAILED;
    }
    GE_CHK_BOOL_EXEC(CheckBufferOptimizeParamValid(FLAGS_buffer_optimize) == SUCCESS, return FAILED,
                     "[Check][BufferOptimize]check output type failed!");

    GE_CHK_BOOL_EXEC(CheckEnableSingleStreamParamValid(std::string(FLAGS_enable_single_stream)) == SUCCESS,
                     return FAILED, "[Check][EnableSingleStream]failed!");

    GE_CHK_BOOL_EXEC(CheckExternalWeightParamValid(std::string(FLAGS_external_weight)) == SUCCESS, return FAILED,
                     "[Check][ExternalWeight]failed!");

    GE_CHK_BOOL_EXEC(CheckAcParallelEnableParamValid(std::string(FLAGS_ac_parallel_enable)) == SUCCESS, return FAILED,
                     "[Check][AcParallelEnable] failed!");
    GE_CHK_BOOL_EXEC(CheckTilingScheduleOptimizeParamValid(std::string(FLAGS_tiling_schedule_optimize)) == SUCCESS,
                     return FAILED, "[Check][TilingScheduleOptimize] failed!");

    is_invalid_input = ((FLAGS_mode == RunMode::GEN_EXE_OM_FOR_NANO) || (FLAGS_mode == RunMode::GEN_EXE_OM)) &&
                       (FLAGS_display_model_info == "1");
    if (is_invalid_input) {
      REPORT_PREDEFINED_ERR_MSG(
          "E10001", std::vector<const char_t *>({"parameter", "value", "reason"}),
          std::vector<const char_t *>({"--display_model_info", FLAGS_display_model_info.c_str(),
                                       "Parameter display_model_info does not support execute-om for nano."}));
      GELOGE(FAILED, "[Check][Parameter]Input parameter[--display_model_info] does not support execute-om nano.");
      return FAILED;
    }

    if (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_OM2_MODEL)) {
      // OM2: 跳过 OPP 白名单校验，依赖 CheckOm2HostEnvValid（方向检查）+ 编译阶段自然报错
      GE_ASSERT_SUCCESS(CheckOm2UserOptionsValid(ge::flgs::GetUserOptions()), "[Check][OM2][UserOptions] failed!");
      GE_ASSERT_SUCCESS(ge::CheckOm2HostEnvValid(FLAGS_host_env_os, FLAGS_host_env_cpu),
                        "[Check][OM2][HostEnv] failed!");
      if (!FLAGS_insert_op_conf.empty()) {
        GE_CHK_BOOL_EXEC(ge::InsertAippOpUtil::ValidateStaticAippOnly(FLAGS_insert_op_conf) == ge::SUCCESS,
                         return FAILED, "[Check][OM2][InsertOpConf] Dynamic AIPP is not supported in OM2 mode.");
      }
    }

    GE_ASSERT_SUCCESS(CheckAllowHF32ParamValid(FLAGS_allow_hf32), "[Check][AllowHF32]failed!");
    GE_ASSERT_SUCCESS(CheckQuantDumpableParamValid(FLAGS_quant_dumpable), "[Check][QuantDumpable] failed!");
    GE_CHK_BOOL_EXEC(CheckAttrCompressionParamValid(FLAGS_enable_attr_compression) == SUCCESS, return FAILED,
                     "[Check][AttrCompression]failed!");
    if (!FLAGS_static_model_ops_lower_limit.empty()) {
      GE_ASSERT_SUCCESS(CheckValidValueRange("--static_model_ops_lower_limit", FLAGS_static_model_ops_lower_limit, -1L,
                                             std::numeric_limits<int64_t>::max()),
                        "[Check][StaticModelOpsLowerLimit]failed!");
    }
    return SUCCESS;
  }

  /**
   * Verifying the parameters of converting model to JSON
   * 1. Fmk_model
   * 2. out_json
   **/
  static Status CheckConverJsonParamFlags() {
    Status ret = SUCCESS;

    // No model path passed in
    if (FLAGS_om.empty()) {
      REPORT_PREDEFINED_ERR_MSG("E10004", std::vector<const char *>({"parameter"}), std::vector<const char *>({"om"}));
      GELOGE(FAILED, "[Check][Parameter]Input parameter[--om]'s value is empty!!");
      ret = FAILED;
    }

    // JSON path not passed in
    if (FLAGS_json.empty()) {
      REPORT_PREDEFINED_ERR_MSG("E10004", std::vector<const char *>({"parameter"}),
                                std::vector<const char *>({"json"}));
      GELOGE(FAILED, "[Check][Parameter]Input parameter[--json]'s value is empty!!");
      ret = FAILED;
    }

    // Check if the model path is valid
    if ((!FLAGS_om.empty()) && (!CheckInputPathValid(FLAGS_om, "--om"))) {
      GELOGE(FAILED, "[Check][InputPath]model file path is invalid: %s.", FLAGS_om.c_str());
      ret = FAILED;
    }

    // Check whether the JSON path is valid
    if ((!FLAGS_json.empty()) && (!CheckOutputPathValid(FLAGS_json, "--json"))) {
      GELOGE(FAILED, "[Check][OutputPath]json file path is invalid: %s.", FLAGS_json.c_str());
      ret = FAILED;
    }

    return ret;
  }

  static bool CheckSocVersionAndRunmode() {
    static const std::map<int32_t, std::vector<std::string>> rule_map = {
        {static_cast<int32_t>(RunMode::GEN_EXE_OM_FOR_NANO), {"Ascend035", "Ascend035A", "Ascend035B"}}};
    for (auto iter = rule_map.begin(); iter != rule_map.end(); iter++) {
      std::string target_soc = " ";
      std::stringstream ss_err_msg;
      if (iter->first == FLAGS_mode) {
        // mode参数匹配成功，校验soc version是否匹配正确
        for (const std::string &soc_str : iter->second) {
          if (soc_str == FLAGS_soc_version) {
            return true;
          }
          target_soc += soc_str + " ";
        }
        ss_err_msg << "Option soc_version " << target_soc << " and mode " << iter->first << " must be set together";
        REPORT_PREDEFINED_ERR_MSG("E10055", std::vector<const char *>({"reason"}),
                                  std::vector<const char *>({ss_err_msg.str().c_str()}));
        GELOGE(FAILED, "[Check][Option]mode[%d] should set soc_version[%s]", iter->first, target_soc.c_str());
        return false;
      } else {
        // soc version匹配成功，但mode参数不匹配
        for (const std::string &soc_str : iter->second) {
          if (soc_str == FLAGS_soc_version) {
            ss_err_msg << "Option soc_version " << soc_str << " and mode " << iter->first << " must be set together";
            REPORT_PREDEFINED_ERR_MSG("E10055", std::vector<const char *>({"reason"}),
                                      std::vector<const char *>({ss_err_msg.str().c_str()}));
            GELOGE(FAILED, "[Check][Option]soc_version[%s] should set mode[%d]", soc_str.c_str(), iter->first);
            return false;
          }
        }
      }
    }
    return true;
  }

 private:
  static bool CheckPathWithName(const std::string &fileName) {
    // Determine file path length
    if (fileName.size() > static_cast<int32_t>(PATH_MAX)) {
      REPORT_PREDEFINED_ERR_MSG("E10021", std::vector<const char *>({"parameter", "size"}),
                                std::vector<const char *>({"output", std::to_string(PATH_MAX).c_str()}));
      GELOGE(FAILED, "[Check][Path]Input parameter[--output]'s path is too long, it must be less than %d", PATH_MAX);
      return false;
    }

    // Find the last separator
    int32_t slashPosition = fileName.size() - 1;
    for (; slashPosition >= 0; slashPosition--) {
      if (fileName[slashPosition] == '\\' || fileName[slashPosition] == '/') {
        break;
      }
    }

    // Failure if no filename follows the path
    if (slashPosition == static_cast<int32_t>(fileName.size() - 1)) {
      REPORT_PREDEFINED_ERR_MSG("E10022", std::vector<const char *>({"parameter", "filename"}),
                                std::vector<const char *>({"output", fileName.c_str()}));
      DOMI_LOGE("Input parameter[--output]'s path[%s] not include file name", fileName.c_str());
      return false;
    }

    return true;
  }

  static flgs::GfStatus GenAndRegOptimizationOptions(std::vector<std::string> &oo_help_info) {
    oo_help_info.resize(static_cast<size_t>(OoCategory::kEnd));
    const auto &visible_opt_table = OptionRegistry::GetInstance().GetVisibleOptions(OoEntryPoint::kAtc);
    for (const auto &opt : visible_opt_table) {
      (void)ge::flgs::RegisterParamString(opt.first, "false", opt.second.help_text);
      const auto iter = opt.second.show_infos.find(OoEntryPoint::kAtc);
      GE_ASSERT_TRUE(iter != opt.second.show_infos.end(), "option [%s] not register help info", opt.first.c_str());
      oo_help_info[static_cast<size_t>(iter->second.catagory)]
          .append("  --")
          .append(opt.first)
          .append("           ")
          .append(opt.second.help_text)
          .append("\n");
    }
    return flgs::GF_SUCCESS;
  }
};

bool IsLegacySoFile(const std::string &file_path) {
  return file_path.size() < std::strlen(kLegacySoSuffix) ||
         file_path.compare((file_path.size() - std::strlen(kLegacySoSuffix)), std::strlen(kLegacySoSuffix),
                           kLegacySoSuffix) != 0;
}

namespace {
void SetDynamicInputSizeOptions() {
  if (!FLAGS_dynamic_batch_size.empty()) {
    domi::GetContext().dynamic_batch_size = FLAGS_dynamic_batch_size;
  }
  if (!FLAGS_dynamic_image_size.empty()) {
    domi::GetContext().dynamic_image_size = FLAGS_dynamic_image_size;
  }
  if (!FLAGS_dynamic_dims.empty()) {
    domi::GetContext().dynamic_dims = FLAGS_dynamic_dims;
  }
}

/// Validate the non-general scope fusion pass.
/// The parameter is set to the name of the fusion rule.
/// Multiple names can be set and separated by ",".
void SetEnableScopeFusionPasses(const std::string &pass_names) {
  GetParserContext().enable_scope_fusion_passes = pass_names;
}

static bool CheckInputFormat() {
  // Set default format
  if (FLAGS_input_format.empty()) {
    if (FLAGS_framework == static_cast<int32_t>(domi::TENSORFLOW)) {
      FLAGS_input_format = "NHWC";
    } else {
      FLAGS_input_format = "NCHW";
    }
    return true;
  }
  if ((FLAGS_framework == static_cast<int32_t>(domi::CAFFE))) {  // caffe
    if (caffe_support_input_format.find(FLAGS_input_format) != caffe_support_input_format.end()) {
      return true;
    }
    // only support NCHW ND
    REPORT_PREDEFINED_ERR_MSG(
        "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
        std::vector<const char *>({"--input_format", FLAGS_input_format.c_str(), kCaffeFormatSupport}));
    GELOGE(FAILED, "[Check][InputFormat]Invalid value for --input_format[%s], %s.", FLAGS_input_format.c_str(),
           kCaffeFormatSupport);
    return false;
  }
  if ((FLAGS_framework == static_cast<int32_t>(domi::TENSORFLOW))) {  // tf
    if (tf_support_input_format.find(FLAGS_input_format) != tf_support_input_format.end()) {
      return true;
    }
    // only support NCHW NHWC ND NCDHW NDHWC
    REPORT_PREDEFINED_ERR_MSG(
        "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
        std::vector<const char *>({"--input_format", FLAGS_input_format.c_str(), kTFFormatSupport}));
    GELOGE(FAILED, "[Check][InputFormat]Invalid value for --input_format[%s], %s.", FLAGS_input_format.c_str(),
           kTFFormatSupport);
    return false;
  }
  if (FLAGS_framework == static_cast<int32_t>(domi::ONNX)) {
    if (onnx_support_input_format.find(FLAGS_input_format) != onnx_support_input_format.end()) {
      return true;
    }
    // only support NCHW ND
    REPORT_PREDEFINED_ERR_MSG(
        "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
        std::vector<const char *>({"--input_format", FLAGS_input_format.c_str(), kONNXFormatSupport}));
    GELOGE(FAILED, "[Check][InputFormat]Invalid value for --input_format[%s], %s.", FLAGS_input_format.c_str(),
           kONNXFormatSupport);
    return false;
  }
  return true;
}

#if !defined(__ANDROID__) && !defined(ANDROID)
static void GetCustomOpPath(std::string &customop_path) {
  GELOGI("Enter get custom op path schedule");
  std::string fmk_type = TypeUtilsInner::FmkTypeToSerialString(static_cast<domi::FrameworkType>(FLAGS_framework));
  GELOGI("Framework type is %s.", fmk_type.c_str());

  Status ret = PluginManager::GetCustomOpPath(fmk_type, customop_path);
  if (ret != SUCCESS) {
    GELOGW("Failed to get custom op path!");
  }
}

void GetPluginSoFileList(const std::string &path, std::vector<std::string> &fileList, std::string &caffe_parser_path) {
  // Support to split multiple so directories by ":"
  GELOGI("path is %s", path.c_str());
  std::vector<std::string> v_path = StringUtils::Split(path, ':');
  for (size_t i = 0; i < v_path.size(); ++i) {
    FindParserSo(v_path[i], fileList, caffe_parser_path);
    GELOGI("CustomOpLib full name = %s", v_path[i].c_str());
  }
}

void LoadModelParserLib(std::string caffe_parser_path) {
  if (FLAGS_framework == static_cast<int32_t>(domi::TENSORFLOW)) {
    void *tf_handle = dlopen("libfmk_parser.so", RTLD_NOW | RTLD_GLOBAL);
    if (tf_handle == nullptr) {
      GELOGW("dlopen fmk library [libfmk_parser.so] failed.");
      return;
    }
    GELOGI("plugin load libfmk_parser.so success.");
  } else if (FLAGS_framework == static_cast<int32_t>(domi::CAFFE)) {
    // What we are dealing with here is that the user modifies the caffe.proto scenario.
    // If no lib_Caffe_Parser.so is found under the plugin path, use the default lib_Caffe_Parser.so path.
    caffe_parser_path = caffe_parser_path.empty() ? "lib_caffe_parser.so" : caffe_parser_path;

    void *handle = dlopen(caffe_parser_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (handle == nullptr) {
      GELOGW("dlopen failed, plugin name:%s. Message(%s).", caffe_parser_path.c_str(), dlerror());
      return;
    }
    GELOGI("plugin load %s success.", caffe_parser_path.c_str());
    // According to the dependency, the Caffe parsing module of the framework is loaded here( libfmk_parser.so).
    // (depend on the lib_caffe_parser.so)
    void *fmk_handle = dlopen("libfmk_parser.so", RTLD_NOW | RTLD_GLOBAL);
    if (fmk_handle == nullptr) {
      GELOGW("dlopen fmk library [libfmk_parser.so] failed.");
      if (dlclose(handle) != 0) {
        GELOGW("dlclose lib_caffe_parser.so failed.");
      }
      return;
    }
    GELOGI("plugin load libfmk_parser.so success.");
  } else if (FLAGS_framework == static_cast<int32_t>(domi::ONNX)) {
    void *handle = dlopen("libfmk_onnx_parser.so", RTLD_NOW | RTLD_GLOBAL);
    if (handle == nullptr) {
      GELOGW("dlopen fmk library [libfmk_onnx_parser.so] failed.");
      return;
    }
    GELOGI("plugin load libfmk_onnx_parser.so success.");
  } else {
    GELOGW("Framework:%s is not supported.",
           TypeUtilsInner::FmkTypeToSerialString(static_cast<domi::FrameworkType>(FLAGS_framework)).c_str());
    return;
  }
  return;
}

void LoadCustomOpLib(bool need_load_ops_plugin) {
  std::string plugin_path;
  GetCustomOpPath(plugin_path);

  std::vector<std::string> fileList;
  std::string caffe_parser_path;

  // whether there are files in the plugin so path
  GetPluginSoFileList(plugin_path, fileList, caffe_parser_path);

  // sort, move "_legacy.so" to the end
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);

  // no file
  if (fileList.empty() && caffe_parser_path.empty()) {
    GELOGW("cannot find any plugin file in plugin_path: %s", plugin_path.c_str());
  }

  LoadModelParserLib(caffe_parser_path);
  if (!need_load_ops_plugin) {
    GELOGI("No need to load ops plugin so.");
    return;
  }
  domi::OpRegistry::Instance()->registrationDatas.clear();
  // load other so files except lib_caffe_parser.so in the plugin so path
  for (auto elem : fileList) {
    StringUtils::Trim(elem);
    // different plugin so may invoke same name func, need RTLD_LOCAL dlopen to avoid invoke same func ptr
    void *handle = dlopen(elem.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
      GELOGW("dlopen failed, plugin name:%s. Message(%s).", elem.c_str(), dlerror());
    } else {
      GELOGI("plugin load %s success.", elem.c_str());
    }
  }

  std::vector<domi::OpRegistrationData> registrationDatas = domi::OpRegistry::Instance()->registrationDatas;
  for (domi::OpRegistrationData reg_data : registrationDatas) {
    if (reg_data.GetFrameworkType() == static_cast<domi::FrameworkType>(FLAGS_framework)) {
      (void)OpRegistrationTbe::Instance()->Finalize(reg_data);
      (void)domi::OpRegistry::Instance()->Register(reg_data);
    }
  }
}

void SaveCustomCaffeProtoPath() {
  GELOGI("Enter save custom caffe proto path.");

  std::string path_base = GELib::GetPath();
  GELOGI("path_base is %s", path_base.c_str());
  path_base = path_base.substr(0, path_base.rfind('/'));
  path_base = path_base.substr(0, path_base.rfind('/') + 1);
  GetParserContext().caffe_proto_path = path_base + "include/proto/";

  std::string customcaffe_path;
  (void)PluginManager::GetCustomCaffeProtoPath(customcaffe_path);
  GetParserContext().custom_proto_path = customcaffe_path;
}

#endif

Status ConstructAndUpdateInputTensors(const OpDescPtr &op, const int64_t index, std::vector<GeTensor> &inputs) {
  GE_CHECK_NOTNULL(op);
  const std::string &data_op_name = op->GetName();
  GE_ASSERT_TRUE(index < static_cast<int64_t>(inputs.size()),
                 "input[%s] data index[%" PRId64 "] out of inputs range[%zu]", data_op_name.c_str(), index,
                 inputs.size());
  GELOGI("Begin to build input tensor for Data op[%s], index %" PRId64, data_op_name.c_str(), index);
  const GeTensorDesc data_tensor_desc = op->GetInputDesc(0);
  GeShape data_shape;
  const auto iter = domi::GetContext().input_dims.find(data_op_name);
  if (iter != domi::GetContext().input_dims.end()) {
    data_shape = GeShape(iter->second);
    GELOGI("Refresh Data op[%s] from context, input shape [%s]", op->GetNamePtr(), data_shape.ToString().c_str());
    GE_CHECK_NOTNULL(op->MutableInputDesc(0));
    op->MutableInputDesc(0)->SetShape(data_shape);
    GE_CHECK_NOTNULL(op->MutableOutputDesc(0));
    op->MutableOutputDesc(0)->SetShape(data_shape);
  } else {
    data_shape = data_tensor_desc.GetShape();
    GELOGI("Data op[%s] get shape [%s] from InputDesc in geir graph.", op->GetNamePtr(), data_shape.ToString().c_str());
  }

  const DataType data_type = data_tensor_desc.GetDataType();
  const std::string data_type_str = TypeUtils::DataTypeToSerialString(data_type);
  GELOGI("Data op[%s] get data type:%s from InputDesc in geir graph.", op->GetNamePtr(), data_type_str.c_str());

  GeTensorDesc desc;
  // 输入私有格式特性在MS和torchair的在线场景都有使能
  // 存量网络中ms导出的air模型携带了私有格式配置，但离线推理不使能私有格式
  // torchair导出的air模型也携带了私有格式配置，希望在离线推理使能输入私有格式。因此通过该属性是否存在，用来区分离线推理是否使能私有格式
  // 该属性为torchair在私有格式场景配置的，用于配置输入的私有格式是否扩散
  bool has_storage_format_spread_attr = AttrUtils::HasAttr(op, "_enable_storage_format_spread");
  bool is_origin_format_set = false;
  (void)AttrUtils::GetBool(data_tensor_desc, ATTR_NAME_ORIGIN_FORMAT_IS_SET, is_origin_format_set);
  if (has_storage_format_spread_attr && data_tensor_desc.IsOriginShapeInitialized() && is_origin_format_set) {
    GELOGI(
        "Input %s[%ld] enable running format. StorageFormat[%s], OriginFormat[%s], StorageShape[%s], OriginShape[%s]",
        op->GetNamePtr(), index, ge::TypeUtils::FormatToSerialString(data_tensor_desc.GetFormat()).c_str(),
        ge::TypeUtils::FormatToSerialString(data_tensor_desc.GetOriginFormat()).c_str(), data_shape.ToString().c_str(),
        data_tensor_desc.GetOriginShape().ToString().c_str());
    desc.SetShape(data_shape);
    desc.SetOriginShape(data_tensor_desc.GetOriginShape());
    desc.SetFormat(data_tensor_desc.GetFormat());
    desc.SetOriginFormat(data_tensor_desc.GetOriginFormat());
    (void)AttrUtils::SetBool(desc, ATTR_NAME_ORIGIN_FORMAT_IS_SET, true);
    desc.SetDataType(data_type);
  } else {
    desc.SetShape(data_shape);
    desc.SetFormat(Format(domi::GetContext().format));
    desc.SetDataType(data_type);
  }
  GeTensor input_tensor;
  input_tensor.SetTensorDesc(desc);
  auto normalized_tensor = TensorAdapter::NormalizeGeTensor(input_tensor);
  GELOGI("Normalize input %s[index:%ld] %s[%s] -> %s[%s]", op->GetNamePtr(), index,
         ge::TypeUtils::FormatToSerialString(input_tensor.GetTensorDesc().GetFormat()).c_str(),
         input_tensor.GetTensorDesc().GetShape().ToString().c_str(),
         ge::TypeUtils::FormatToSerialString(normalized_tensor.MutableTensorDesc().GetFormat()).c_str(),
         normalized_tensor.MutableTensorDesc().GetShape().ToString().c_str());
  inputs[index] = std::move(normalized_tensor);

  return SUCCESS;
}

Status CreateInputsForInference(const Graph &graph, std::vector<GeTensor> &inputs) {
  const auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  GE_CHECK_NOTNULL(compute_graph);
  std::vector<OpDescPtr> data_op_desc;
  std::set<int64_t> indexes;
  for (const auto &node : compute_graph->GetDirectNode()) {
    GE_ASSERT_NOTNULL(node);
    auto op = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op);
    if (OpTypeUtils::IsDataNode(op->GetType())) {
      int64_t index = std::numeric_limits<int64_t>::max();
      (void)AttrUtils::GetInt(op, ATTR_NAME_INDEX, index);
      (void)indexes.insert(index);
      data_op_desc.emplace_back(op);
    }
  }

  if (data_op_desc.empty()) {
    // 没有 Data 节点, 跳过下面的处理流程
    return SUCCESS;
  }

  const auto min_index = indexes.begin();
  auto max_index = indexes.end();
  --max_index;
  const auto data_op_size = static_cast<int64_t>(data_op_desc.size());
  if (indexes.size() != data_op_desc.size() || *min_index != 0 || *max_index != data_op_size - 1) {
    GELOGI("Graph[%s] has invalid input data index, set data index by topo order", compute_graph->GetName().c_str());
    int64_t index = 0;
    for (auto &op : data_op_desc) {
      (void)AttrUtils::SetInt(op, ATTR_NAME_INDEX, index++);
    }
  }

  inputs.resize(data_op_desc.size());
  for (const auto &input_op : data_op_desc) {
    int64_t index = 0;
    GE_ASSERT_TRUE(AttrUtils::GetInt(input_op, ATTR_NAME_INDEX, index));
    GE_ASSERT_SUCCESS(ConstructAndUpdateInputTensors(input_op, index, inputs),
                      "Construct input tensor failed, op[%s], index[%" PRId64 "]", input_op->GetNamePtr(), index);
  }
  GELOGI("Build ME model, inputs size is: %zu", inputs.size());
  return SUCCESS;
}

Status GenerateInfershapeJson() {
  if (!CheckInputFormat()) {
    GELOGE(FAILED, "[Check][InputFormat] failed.");
    return FAILED;
  }

  GeGenerator ge_generator;
  std::map<std::string, std::string> options;
  Status ret = ge_generator.Initialize(options, domi::GetContext());
  if (ret != SUCCESS) {
    DOMI_LOGE("GeGenerator initialize failed!");
    return FAILED;
  }
  GE_MAKE_GUARD(release_python_runtime, []() { (void)GePythonRuntimeManager::Instance().ShutdownProcess(); });

  Graph graph;
  std::map<std::string, std::string> atc_params;
  atc_params.insert(std::pair<std::string, std::string>("input_format", FLAGS_input_format));
  atc_params.insert(std::pair<std::string, std::string>("check_report", FLAGS_check_report));
  ret = ParseGraph(graph, atc_params, FLAGS_om.c_str(), FLAGS_weight.c_str(),
                   static_cast<domi::FrameworkType>(FLAGS_framework), "", FLAGS_target.c_str(),
                   static_cast<RunMode>(FLAGS_mode), false);
  if (ret != SUCCESS) {
    DOMI_LOGE("ATC Parse graph FAILED");
    (void)ge_generator.Finalize();
    return FAILED;
  }

  ret = ge_generator.GenerateInfershapeGraph(graph);
  if (ret != SUCCESS) {
    DOMI_LOGE("ATC GenerateInfershapeJson failed");
    (void)ge_generator.Finalize();
    return FAILED;
  }
  if (DumpInfershapeJson(graph, FLAGS_json.c_str()) != SUCCESS) {
    DOMI_LOGE("ATC DumpInfershapeJson failed");
    (void)ge_generator.Finalize();
    return FAILED;
  }
  (void)ge_generator.Finalize();
  return SUCCESS;
}

static Status ConvertModelToJson(int32_t fwk_type, const std::string &model_file, const std::string &json_file) {
  Status ret = SUCCESS;
  if (fwk_type == -1) {
    ret = ConvertOm(model_file.c_str(), json_file.c_str(), true);
    return ret;
  }
  GE_ASSERT_GRAPH_SUCCESS(OpLibRegistry::GetInstance().PreProcessForCustomOp());
  // Need to save caffe.proto path
  SaveCustomCaffeProtoPath();

  if (FLAGS_dump_mode == "0") {
    // Caffe or tf model to json depend on lib_caffe_parser.so or libfmk_parser.so.
    LoadCustomOpLib(false);
    ret = ConvertFwkModelToJson(static_cast<domi::FrameworkType>(fwk_type), model_file.c_str(), json_file.c_str());
  } else if (FLAGS_dump_mode == "1") {
    // Caffe or tf model to json depend on lib_caffe_parser.so or libfmk_parser.so and ops plugin so.
    LoadCustomOpLib(true);
    ret = GenerateInfershapeJson();
  }

  return ret;
}

static Status SetAttrOptions(Graph &graph) {
  if (!FLAGS_keep_dtype.empty()) {
    if (aclgrphSetOpAttr(graph, ATTR_TYPE_KEEP_DTYPE, FLAGS_keep_dtype.c_str()) != GRAPH_SUCCESS) {
      return FAILED;
    }
  }
  if (!FLAGS_compress_weight_conf.empty()) {
    if (aclgrphSetOpAttr(graph, ATTR_TYPE_WEIGHT_COMPRESS, FLAGS_compress_weight_conf.c_str()) != GRAPH_SUCCESS) {
      return FAILED;
    }
  }

  return SUCCESS;
}
}  // namespace

Status CallAmctInterface(Graph &graph, std::map<std::string, std::string> &options) {
  auto it = options.find(std::string(ge::COMPRESSION_OPTIMIZE_CONF));
  if ((it != options.end()) && (!it->second.empty())) {
    options.insert(std::pair<std::string, std::string>("build_graph_already_initialized", "1"));
    void* handle = mmDlopen(kAmctSo, static_cast<int32_t>(MMPA_RTLD_NOW));
    if (handle == nullptr) {
      const char_t *error = mmDlerror();
      error = (error == nullptr) ? "" : error;
      string errmsg = "AMCT is not installed, error: " + string(error);
      (void)REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
            std::vector<const char *>({"--compression_optimize_conf", it->second.c_str(), errmsg.c_str()}));
      GELOGE(FAILED, "[Open][AmctSo] %s failed, error: %s", kAmctSo, error);
      return FAILED;
    }
    GE_MAKE_GUARD(close_handle, [&handle]() {
      // dlClose 前先析构 IAmctCalibration
      GeAmctRegistry::GetInstance().Unregister();
      (void)mmDlclose(handle);
    });

    const auto ret = GeAmctRegistry::GetInstance().Invoke(graph, options);
    if (ret != GRAPH_PARAM_INVALID) {
      if ((ret != GRAPH_NOT_CHANGED) && (ret != GRAPH_SUCCESS)) {
        GELOGE(FAILED, "ATC call amct graph calibration interface failed");
        return FAILED;
      }
      GELOGI("Enable AMCT via amct calibration class succeeded.");
      return SUCCESS;
    }

    // 计划：需要待AMCT组件合入后，再删除原始流程。
    const auto amctGraphCalibration =
        reinterpret_cast<amctStatus (*)(ge::Graph &, const std::map<std::string, std::string> &)>(
            mmDlsym(handle, "amctGraphCalibration"));
    if (amctGraphCalibration == nullptr) {
      DOMI_LOGE("[Check][Param] Symbol amctGraphCalibration can't find in %s", kAmctSo);
      return FAILED;
    }

    const graphStatus amct_ret = static_cast<graphStatus>(amctGraphCalibration(graph, options));
    if ((amct_ret != GRAPH_NOT_CHANGED) && (amct_ret != GRAPH_SUCCESS)) {
      DOMI_LOGE("ATC call amctGraphCalibration interface failed");
      return FAILED;
    }
  }
  return SUCCESS;
}

namespace {
static Status GenerateOfflineModel(GeGenerator &ge_generator, Graph graph, std::string output,
                                   std::vector<GeTensor> inputs) {
  std::map<int32_t, OfflineModelFormat> flags_mode_map = {
      {GEN_EXE_OM_FOR_NANO, OfflineModelFormat::OM_FORMAT_NANO},
      {GEN_OM2_MODEL, OfflineModelFormat::OM_FORMAT_OM2},
  };

  if (flags_mode_map.find(FLAGS_mode) != flags_mode_map.end()) {
    return ge_generator.GenerateOfflineModel(graph, output, inputs, flags_mode_map[FLAGS_mode]);
  }
  return ge_generator.GenerateOfflineModel(graph, output, inputs, OfflineModelFormat::OM_FORMAT_DEFAULT);
}

void SetAtcParams(std::map<std::string, std::string> &atc_params, const std::string &output) {
  atc_params.insert(std::pair<std::string, std::string>("input_shape", FLAGS_input_shape));
  atc_params.insert(std::pair<std::string, std::string>(INPUT_SHAPE_RANGE, FLAGS_input_shape_range));
  atc_params.insert(std::pair<std::string, std::string>("out_nodes", FLAGS_out_nodes));
  atc_params.insert(std::pair<std::string, std::string>("input_format", FLAGS_input_format));
  atc_params.insert(std::pair<std::string, std::string>("check_report", FLAGS_check_report));
  atc_params.insert(std::pair<std::string, std::string>("input_fp16_nodes", FLAGS_input_fp16_nodes));
  atc_params.insert(std::pair<std::string, std::string>("is_input_adjust_hw_layout", FLAGS_is_input_adjust_hw_layout));
  atc_params.insert(
      std::pair<std::string, std::string>("is_output_adjust_hw_layout", FLAGS_is_output_adjust_hw_layout));
  atc_params.insert(std::pair<std::string, std::string>(string(OUTPUT_DATATYPE), FLAGS_output_type));
  atc_params.insert(std::pair<std::string, std::string>("output", output));
}

Status GenerateModelBySingleGraph(GeGenerator &ge_generator, const std::string &output,
                                  std::map<std::string, std::string> &options) {
  Graph graph;
  std::vector<GeTensor> inputs;
  Status ret = SUCCESS;
  if (FLAGS_framework == domi::MINDSPORE) {
    // load model from file
    Model load_model = Model("loadmodel", "version2");
    auto ret1 = load_model.LoadFromFile(FLAGS_model);
    if (ret1 != GRAPH_SUCCESS) {
      REPORT_PREDEFINED_ERR_MSG("E10041", std::vector<const char_t *>({"parameter"}),
                                std::vector<const char_t *>({FLAGS_model.c_str()}));
      DOMI_LOGE(
          "Load model from %s failed, please check model file or "
          "input parameter[--framework] is correct",
          FLAGS_model.c_str());
      return FAILED;
    }

    graph = GraphUtilsEx::CreateGraphFromComputeGraph(load_model.GetGraph());

    GE_CHK_STATUS_EXEC(InitDomiOmgContext(FLAGS_input_shape, FLAGS_input_format, "", is_dynamic_input), return FAILED,
                       "[Init][DomiOmgContext]ATC Generate call InitDomiOmgContext ret fail");
    GE_ASSERT_SUCCESS(CheckParamForAirInput(graph));
    ret = CreateInputsForInference(graph, inputs);
    if (ret != SUCCESS) {
      GELOGE(FAILED, "[Create][InputsForInference] failed.");
      const std::string reason = "Failed to create inputs for inference from --graph and --inputs.";
      REPORT_PREDEFINED_ERR_MSG("E10003", std::vector<const char *>({"value", "parameter", "reason"}),
                                std::vector<const char *>({"N/A", "graph", reason.c_str()}));
      return FAILED;
    }
  } else {
    std::map<std::string, std::string> atc_params;
    SetAtcParams(atc_params, output);
    ret = ParseGraph(graph, atc_params, FLAGS_model.c_str(), FLAGS_weight.c_str(),
                     static_cast<domi::FrameworkType>(FLAGS_framework), FLAGS_op_name_map.c_str(), FLAGS_target.c_str(),
                     static_cast<RunMode>(FLAGS_mode), is_dynamic_input);

    // in ONLY_PRE_CHECK mode, pre-checking report has already saved in ParseGraph
    if (FLAGS_mode == static_cast<int32_t>(RunMode::ONLY_PRE_CHECK)) {
      if (ret != SUCCESS) {
        DOMI_LOGE("ATC precheck fail.");
        return FAILED;
      }
      return SUCCESS;
    }

    if (ret != SUCCESS) {
      DOMI_LOGE("ATC Parse graph FAILED");
      DOMI_LOGE("ATC Generate execute failed");  // Duplicate log. (for test case
      return FAILED;
    }
    if (SetOutputNodeInfo(graph, FLAGS_output_type) != SUCCESS) {
      DOMI_LOGE("Set output node info fail.");
      return FAILED;
    }
  }

  if (SetAttrOptions(graph) != SUCCESS) {
    return FAILED;
  }

  GE_CHK_STATUS_EXEC(CallAmctInterface(graph, options), return FAILED,
                     "[Call][AmctInterface]ATC Generate call AmctInterface ret fail");
  ret = GenerateOfflineModel(ge_generator, graph, output, inputs);
  if (ret != SUCCESS) {
    REPORT_INNER_ERR_MSG("E19999", "GE GenerateOfflineModel execute failed.");
    DOMI_LOGE("GE GenerateOfflineModel execute failed");
    return FAILED;
  }
  return SUCCESS;
}

Status GenerateModel(std::map<std::string, std::string> &options, const std::string &output) {
  GeGenerator ge_generator;
  Status ret = SUCCESS;
  std::shared_ptr<GELib> instance_ptr = GELib::GetInstance();
  bool release_python_runtime = false;
  GE_DISMISSABLE_GUARD(release_python_runtime_guard, ([&release_python_runtime]() {
    if (release_python_runtime) {
      (void)GePythonRuntimeManager::Instance().ShutdownProcess();
    }
  }));
  if (instance_ptr == nullptr || !instance_ptr->InitFlag()) {
    GE_ASSERT_SUCCESS(GePythonRuntimeManager::Instance().EnsureReady());
    release_python_runtime = true;
    ret = GELib::Initialize(options);
    if (ret != SUCCESS) {
      DOMI_LOGE("GE initialize failed!");
      return FAILED;
    }
  }
  ret = ge_generator.Initialize(options, domi::GetContext());
  if (ret != SUCCESS) {
    DOMI_LOGE("GeGenerator initialize failed!");
    (void)GELib::GetInstance()->Finalize();
    return FAILED;
  }
  const std::function<void()> callback = [&ge_generator]() {
    (void)ge_generator.Finalize();
    (void)GELib::GetInstance()->Finalize();
    (void)GePythonRuntimeManager::Instance().ShutdownProcess();
  };
  GE_MAKE_GUARD(release, callback);
  release_python_runtime = false;
  GELOGD("Current input is single graph to generate model.");
  return GenerateModelBySingleGraph(ge_generator, output, options);
}

static void SetEnvForSingleOp(std::map<std::string, std::string> &options) {
  std::string flag_on = "1";
  std::string flag_off = "0";
  options.emplace(STREAM_NUM, "1");  // single op only use one stream
  options.emplace(RUN_FLAG, flag_off);
  options.emplace(OPTION_GRAPH_RUN_MODE, flag_off);
  options.emplace(SINGLE_OP_FLAG, flag_on);
  options.emplace(OP_PRECISION_MODE, FLAGS_op_precision_mode);
  options.emplace(ALLOW_HF32, FLAGS_allow_hf32);
  options.emplace(PRECISION_MODE, FLAGS_precision_mode);
  options.emplace(PRECISION_MODE_V2, FLAGS_precision_mode_v2);
  options.emplace(SOC_VERSION, FLAGS_soc_version);
  options.emplace(VIRTUAL_TYPE, std::to_string(FLAGS_virtual_type));
  options.emplace(CORE_TYPE, FLAGS_core_type);
  options.emplace(AICORE_NUM, FLAGS_aicore_num);
  options.emplace(OP_SELECT_IMPL_MODE, FLAGS_op_select_implmode);
  options.emplace(OPTYPELIST_FOR_IMPLMODE, FLAGS_optypelist_for_implmode);
  options.emplace(OP_DEBUG_LEVEL, to_string(FLAGS_op_debug_level));
  options.emplace(DEBUG_DIR, FLAGS_debug_dir);
  options.emplace(OP_COMPILER_CACHE_DIR, FLAGS_op_compiler_cache_dir);
  options.emplace(OP_COMPILER_CACHE_MODE, FLAGS_op_compiler_cache_mode);
  options.emplace(MDL_BANK_PATH_FLAG, FLAGS_mdl_bank_path);
  options.emplace(OP_BANK_PATH_FLAG, FLAGS_op_bank_path);
  options.emplace(TUNE_DEVICE_IDS, FLAGS_device_id);
  options.emplace(MODIFY_MIXLIST, FLAGS_modify_mixlist);
  options.emplace(OPTION_EXEC_HCCL_FLAG, flag_off);
  options.emplace(ENABLE_SMALL_CHANNEL, FLAGS_enable_small_channel);
  options.emplace(ENABLE_SPARSE_MATRIX_WEIGHT, std::to_string(FLAGS_sparsity));
  options.emplace(ATOMIC_CLEAN_POLICY, FLAGS_atomic_clean_policy);
  options.emplace(EXTERNAL_WEIGHT, FLAGS_external_weight);
  options.emplace(DETERMINISTIC, FLAGS_deterministic);
  options.emplace("ge.deterministicLevel", FLAGS_deterministic_level);
  options.emplace(CUSTOMIZE_DTYPES, FLAGS_customize_dtypes);
  options.emplace("ge.is_weight_clip", FLAGS_is_weight_clip);
  // atc do not limit resource ever
  options.insert(std::pair<std::string, std::string>(std::string(EVALUATE_GRAPH_RESOURCE_MODE), std::to_string(1)));
  options.emplace(ge::JIT_COMPILE, FLAGS_jit_compile);
  if (!FLAGS_optimization_switch.empty()) {
    options.emplace(ge::OPTIMIZATION_SWITCH, FLAGS_optimization_switch);
  }
  SetBuildGraphModeOffline(options);
  SetOptionNameMap(options);
}

Status CheckSingleOpOptions() {
  if ((!GFlagUtils::IsRequiredParameterExists("--output", FLAGS_output)) ||
      (!GFlagUtils::IsRequiredParameterExists("--soc_version", FLAGS_soc_version))) {
    return FAILED;
  }
  if (!CheckOutputPathValid(FLAGS_output, "--output")) {
    DOMI_LOGE("output path %s is not valid!", FLAGS_output.c_str());
    return FAILED;
  }
  // check optypelist_for_implmode and op_select_implmode
  if (CheckImplmodeParamValid(FLAGS_optypelist_for_implmode, FLAGS_op_select_implmode) != SUCCESS) {
    GELOGE(FAILED, "[Check][ImplmodeParam] fail for input optypelist_for_implmode and op_select_implmode.");
    return FAILED;
  }

  if (!FLAGS_op_precision_mode.empty() && !CheckInputPathValid(FLAGS_op_precision_mode, "--op_precision_mode")) {
    REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                              std::vector<const char *>({"--op_precision_mode", FLAGS_op_precision_mode.c_str(),
                                                         "Path defined by op_precision_mode is not found."}));
    GELOGE(FAILED, "[Check][op_precision_mode] %s not found", FLAGS_op_precision_mode.c_str());
    return FAILED;
  }

  GE_ASSERT_SUCCESS(GFlagUtils::CheckPrecisionRelatedFlags());
  GE_ASSERT_SUCCESS(CheckAllowHF32ParamValid(FLAGS_allow_hf32), "[Check][AllowHF32]failed!");
  return SUCCESS;
}

Status BuildSingleOpModels(GeGenerator &generator, std::vector<SingleOpBuildParam> &build_params) {
  Status ret = SUCCESS;
  int32_t index = 0;
  for (auto &param : build_params) {
    std::string output_path;
    if (!FLAGS_output.empty()) {
      output_path = FLAGS_output + "/";
    }
    output_path += param.file_name;
    ret = generator.BuildSingleOpModel(param.op_desc, param.inputs, param.outputs, output_path, param.compile_flag);
    if (ret != SUCCESS) {
      DOMI_LOGE("Compile op failed. ge ret = %u, op index = %d", ret, index);
      ret = FAILED;
    } else {
      GELOGI("Compile op success. op index = %d, output = %s", index, output_path.c_str());
    }
    index += 1;
  }
  return ret;
}

Status GenerateSingleOp(const std::string &json_file_path) {
  GE_ASSERT_SUCCESS(CheckSingleOpOptions());
  GE_ASSERT_GRAPH_SUCCESS(OpLibRegistry::GetInstance().PreProcessForCustomOp());
  std::map<std::string, std::string> options;
  // need to be changed when ge.ini plan is done
  SetEnvForSingleOp(options);
  // print single op option map
  PrintOptionMap(options, "single op option");

  GE_ASSERT_SUCCESS(GePythonRuntimeManager::Instance().EnsureReady());
  GE_MAKE_GUARD(release_python_runtime, []() { (void)GePythonRuntimeManager::Instance().ShutdownProcess(); });
  auto ret = GELib::Initialize(options);
  if (ret != SUCCESS) {
    DOMI_LOGE("GE initialize failed!");
    return FAILED;
  }

  GeGenerator generator;
  ret = generator.Initialize(options, domi::GetContext());
  if (ret != SUCCESS) {
    DOMI_LOGE("GeGenerator initialize failed!");
    (void)GELib::GetInstance()->Finalize();
    return FAILED;
  }

  std::vector<SingleOpBuildParam> build_params;
  if (SingleOpParser::ParseSingleOpList(json_file_path, build_params) != SUCCESS) {
    DOMI_LOGE("parse single op json file failed");
    (void)generator.Finalize();
    (void)GELib::GetInstance()->Finalize();
    return FAILED;
  }

  ret = BuildSingleOpModels(generator, build_params);
  (void)generator.Finalize();
  (void)GELib::GetInstance()->Finalize();
  return ret;
}

static Status AppendOptimizationOptions(std::map<std::string, std::string> &options) {
  options.insert(std::pair<std::string, std::string>(OO_LEVEL, FLAGS_oo_level));
  const auto &visible_opt_table = OptionRegistry::GetInstance().GetVisibleOptions(OoEntryPoint::kAtc);
  for (const auto &opt : ge::flgs::GetUserOptions()) {
    const auto iter = visible_opt_table.find(opt.first);
    if (iter == visible_opt_table.end()) {
      continue;
    }
    options.insert(std::pair<std::string, std::string>(iter->second.name, opt.second));
    GELOGD("Insert optimization option:%s with value:%s success", iter->second.name.c_str(), opt.second.c_str());
  }
  // atc流程保持不变，atc有tf/caffe/onnx，O1下不可关闭的pass还是保持打开状态
  auto &optimization_switch = options[ge::OPTIMIZATION_SWITCH];
  if (!optimization_switch.empty()) {
    optimization_switch.append(";");
  }
  optimization_switch.append("forbidden_close_pass:on");
  return SUCCESS;
}

void SetAtcBasicOptions(std::map<std::string, std::string> &options) {
  options.insert(std::pair<std::string, std::string>(std::string(OPTION_GRAPH_RUN_MODE), "0"));
  options.insert(std::pair<std::string, std::string>(std::string(FRAMEWORK_TYPE), std::to_string(FLAGS_framework)));
  options.insert(std::pair<std::string, std::string>(std::string(STREAM_NUM), std::to_string(1)));
  options.insert(std::pair<std::string, std::string>(std::string(CALIBRATION_CONF_FILE), FLAGS_cal_conf));
  options.insert(std::pair<std::string, std::string>(std::string(OUTPUT_NODE_NAME), FLAGS_out_nodes));
  options.insert(std::pair<std::string, std::string>(std::string(INSERT_OP_FILE), FLAGS_insert_op_conf));
  options.insert(std::pair<std::string, std::string>(std::string(OP_PRECISION_MODE), FLAGS_op_precision_mode));
  options.insert(std::pair<std::string, std::string>(std::string(PRECISION_MODE), FLAGS_precision_mode));
  options.insert(std::pair<std::string, std::string>(std::string(PRECISION_MODE_V2), FLAGS_precision_mode_v2));
  options.insert(std::pair<std::string, std::string>(std::string(ALLOW_HF32), FLAGS_allow_hf32));
  options.insert(std::pair<std::string, std::string>(std::string(TUNE_DEVICE_IDS), FLAGS_device_id));
  // atc do not limit resource ever
  options.insert(std::pair<std::string, std::string>(std::string(EVALUATE_GRAPH_RESOURCE_MODE), std::to_string(1)));

  options.insert(std::pair<std::string, std::string>(std::string(RUN_FLAG), std::to_string(0)));
  options.insert(std::pair<std::string, std::string>(std::string(TRAIN_FLAG), std::to_string(0)));
  options.insert(std::pair<std::string, std::string>(std::string(OPTION_EXEC_HCCL_FLAG), std::to_string(1)));
  if (!FLAGS_output_type.empty()) {
    options.insert(std::pair<std::string, std::string>(std::string(OUTPUT_DATATYPE), FLAGS_output_type));
  }
}

void SetAtcTargetOptions(std::map<std::string, std::string> &options) {
  options.insert(std::pair<std::string, std::string>(std::string(OP_SELECT_IMPL_MODE), FLAGS_op_select_implmode));
  options.insert(
      std::pair<std::string, std::string>(std::string(OPTYPELIST_FOR_IMPLMODE), FLAGS_optypelist_for_implmode));

  if (!FLAGS_input_fp16_nodes.empty()) {
    GELOGI("FLAGS_input_fp16_nodes : %s .", FLAGS_input_fp16_nodes.c_str());
    options.insert(std::pair<std::string, std::string>(INPUT_FP16_NODES, FLAGS_input_fp16_nodes));
  }

  options.insert(std::pair<std::string, std::string>(std::string(OPTION_EXEC_DISABLE_REUSED_MEMORY),
                                                     std::to_string(FLAGS_disable_reuse_memory)));

  options.insert(std::pair<std::string, std::string>(std::string(SOC_VERSION), FLAGS_soc_version));
  options.insert(std::pair<std::string, std::string>(std::string(VIRTUAL_TYPE), std::to_string(FLAGS_virtual_type)));
  options.insert(std::pair<std::string, std::string>(std::string(CORE_TYPE), FLAGS_core_type));
  options.insert(std::pair<std::string, std::string>(std::string(AICORE_NUM), FLAGS_aicore_num));
}

void SetAtcTuningOptions(std::map<std::string, std::string> &options) {
  options.insert(std::pair<std::string, std::string>(std::string(BUFFER_OPTIMIZE), FLAGS_buffer_optimize));
  options.insert(std::pair<std::string, std::string>(std::string(ENABLE_SMALL_CHANNEL), FLAGS_enable_small_channel));
  options.insert(std::pair<std::string, std::string>(std::string(FUSION_SWITCH_FILE), FLAGS_fusion_switch_file));
  options.insert(
      std::pair<std::string, std::string>(std::string(ge::COMPRESSION_OPTIMIZE_CONF), FLAGS_compression_optimize_conf));
  options.insert(std::pair<std::string, std::string>(std::string(ge::CUSTOMIZE_DTYPES), FLAGS_customize_dtypes));
  options.insert(std::pair<std::string, std::string>(std::string(OP_DEBUG_CONFIG), FLAGS_op_debug_config));
  options.insert(std::pair<std::string, std::string>(
      std::string(ENABLE_COMPRESS_WEIGHT),
      (FLAGS_enable_compress_weight == "true") ? kEnableCompressWeightTrue : kEnableCompressWeightFalse));
  options.insert(
      std::pair<std::string, std::string>(std::string(ENABLE_SPARSE_MATRIX_WEIGHT), std::to_string(FLAGS_sparsity)));
  options.insert(std::pair<std::string, std::string>(std::string(ENABLE_SINGLE_STREAM), FLAGS_enable_single_stream));
  options.insert(std::pair<std::string, std::string>(std::string(AC_PARALLEL_ENABLE), FLAGS_ac_parallel_enable));
  options.insert(
      std::pair<std::string, std::string>(std::string(TILING_SCHEDULE_OPTIMIZE), FLAGS_tiling_schedule_optimize));
  options.insert(std::pair<std::string, std::string>(std::string(QUANT_DUMPABLE), FLAGS_quant_dumpable));
}

void SetAtcDebugOptions(std::map<std::string, std::string> &options) {
  std::string lower_value = FLAGS_enable_attr_compression;
  std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
  options.insert(std::pair<std::string, std::string>(std::string(ENABLE_ATTR_COMPRESSION), lower_value));

  options.insert(std::pair<std::string, std::string>(std::string(DEBUG_DIR), FLAGS_debug_dir));

  if (!FLAGS_status_check.empty()) {
    FLAGS_status_check = (FLAGS_status_check == "1") ? "true" : "false";
    options.insert(std::pair<std::string, std::string>(std::string(STATUS_CHECK), FLAGS_status_check));
  }

  options.insert(std::pair<std::string, std::string>(std::string(OP_COMPILER_CACHE_DIR), FLAGS_op_compiler_cache_dir));

  options.insert(
      std::pair<std::string, std::string>(std::string(OP_COMPILER_CACHE_MODE), FLAGS_op_compiler_cache_mode));
}

void SetAtcSaveAndBankOptions(std::map<std::string, std::string> &options) {
  SetDynamicInputSizeOptions();
  if (!FLAGS_save_original_model.empty()) {
    options.insert(std::pair<std::string, std::string>(std::string(SAVE_ORIGINAL_MODEL), FLAGS_save_original_model));
    options.insert(
        std::pair<std::string, std::string>(std::string(ORIGINAL_MODEL_FILE), FLAGS_output + "_original.om"));
  }

  options.insert(
      std::pair<std::string, std::string>(std::string(OP_DEBUG_LEVEL), std::to_string(FLAGS_op_debug_level)));
  options.insert(std::pair<std::string, std::string>(std::string(MDL_BANK_PATH_FLAG), FLAGS_mdl_bank_path));
  options.insert(std::pair<std::string, std::string>(std::string(OP_BANK_PATH_FLAG), FLAGS_op_bank_path));
  options.insert(std::pair<std::string, std::string>(std::string(DISPLAY_MODEL_INFO), FLAGS_display_model_info));
  options.insert(std::pair<std::string, std::string>(std::string(MODIFY_MIXLIST), FLAGS_modify_mixlist));
}

void SetAtcEnvironmentOptions(std::map<std::string, std::string> &options) {
  options.insert(std::pair<std::string, std::string>(std::string(SHAPE_GENERALIZED_BUILD_MODE),
                                                     FLAGS_shape_generalized_build_mode));
  options.insert(std::pair<std::string, std::string>(std::string(ATOMIC_CLEAN_POLICY), FLAGS_atomic_clean_policy));
  options.insert(std::pair<std::string, std::string>(std::string(EXTERNAL_WEIGHT), FLAGS_external_weight));
  options.insert(std::pair<std::string, std::string>(std::string(DETERMINISTIC), FLAGS_deterministic));
  options.insert(std::pair<std::string, std::string>("ge.deterministicLevel", FLAGS_deterministic_level));
  options.insert(std::pair<std::string, std::string>(std::string(OPTION_HOST_ENV_OS), FLAGS_host_env_os));
  options.insert(std::pair<std::string, std::string>(std::string(OPTION_HOST_ENV_CPU), FLAGS_host_env_cpu));
  options.insert(std::pair<std::string, std::string>("ge.is_weight_clip", FLAGS_is_weight_clip));
  options.insert(std::pair<std::string, std::string>(std::string(CLUSTER_CONFIG), FLAGS_cluster_config));
  options.insert(
      std::pair<std::string, std::string>(std::string("ge.hccl_sub_comm_config"), FLAGS_hccl_sub_comm_config));
  if (!FLAGS_build_config.empty()) {
    options.insert(std::pair<std::string, std::string>(std::string(kOptionBuildConfig), FLAGS_build_config));
  }
  if (!FLAGS_input_hint_shape.empty()) {
    options.insert(std::pair<std::string, std::string>(std::string(INPUT_HINT_SHAPE), FLAGS_input_hint_shape));
  }
}

void SetAtcJitOptions(std::map<std::string, std::string> &options) {
  options.insert(std::pair<std::string, std::string>(ge::JIT_COMPILE, FLAGS_jit_compile));
  if (!FLAGS_optimization_switch.empty()) {
    options.insert(std::pair<std::string, std::string>(ge::OPTIMIZATION_SWITCH, FLAGS_optimization_switch));
  }
  if (!FLAGS_static_model_ops_lower_limit.empty()) {
    options.insert(std::pair<std::string, std::string>(ge::OPTION_STATIC_MODEL_OPS_LOWER_LIMIT,
                                                       FLAGS_static_model_ops_lower_limit));
  }
}

Status PrepareAtcOptions(const std::map<std::string, std::string> &raw_options,
                         std::map<std::string, std::string> &options) {
  SetAtcBasicOptions(options);
  SetAtcTargetOptions(options);
  SetAtcTuningOptions(options);
  SetAtcDebugOptions(options);
  SetAtcSaveAndBankOptions(options);
  SetAtcEnvironmentOptions(options);
  SetAtcJitOptions(options);
  MergeRawGeOptions(raw_options, options);

  GE_ASSERT_SUCCESS(AppendOptimizationOptions(options), "Add optimization option failed");
  SetEnableScopeFusionPasses(FLAGS_enable_scope_fusion_passes);
  SetSingleCompileThread(options);
  SetBuildGraphModeOffline(options);
  SetOptionNameMap(options);
  PrintOptionMap(options, "atc option");
  return SUCCESS;
}

void AppendOutputFileSuffix() {
  const auto it = kFilePrefixMap.find(static_cast<RunMode>(FLAGS_mode));
  if (it == kFilePrefixMap.end()) {
    FLAGS_output += kFilePreffix;
  } else {
    FLAGS_output += it->second;
  }
}

Status MaybeDisplayModelInfo() {
  if (FLAGS_display_model_info == "1") {
    GELOGI("need to display model info.");
    return ConvertOm(ModelHelper::GetOutputFileName().c_str(), "", false);
  }
  return SUCCESS;
}

Status PrepareOmGeneration() {
  if (!CheckInputFormat()) {
    GELOGE(FAILED, "[Check][InputFormat]failed.");
    return FAILED;
  }
  SetDefaultHostEnvOsAndHostEnvCpu(FLAGS_host_env_os, FLAGS_host_env_cpu);
  GE_CHK_BOOL_EXEC(GFlagUtils::CheckFlags() == SUCCESS, return FAILED,
                   "[Check][Flags] failed! Please check whether some atc params that include semicolons[;] use double "
                   "quotation marks (\") to enclose each argument such as out_nodes, input_shape, dynamic_image_size");
#if !defined(__ANDROID__) && !defined(ANDROID)
  GE_ASSERT_GRAPH_SUCCESS(OpLibRegistry::GetInstance().PreProcessForCustomOp());
  LoadCustomOpLib(true);
  SaveCustomCaffeProtoPath();
#endif
  return SUCCESS;
}

Status GenerateOmModel(const std::map<std::string, std::string> &raw_options) {
  GE_ASSERT_SUCCESS(PrepareOmGeneration(), "[Prepare][OmGeneration]failed.");
  std::map<std::string, std::string> options;
  GE_ASSERT_SUCCESS(PrepareAtcOptions(raw_options, options), "[Prepare][AtcOptions]failed.");
  AppendOutputFileSuffix();
  GE_CHK_BOOL_EXEC(GenerateModel(options, FLAGS_output) == SUCCESS, return FAILED, "[Generate][Model]failed.");
  return MaybeDisplayModelInfo();
}

Status ConvertModelToJson() {
  Status ret = GFlagUtils::CheckConverJsonParamFlags();
  GE_CHK_BOOL_EXEC(ret == SUCCESS, return FAILED, "[CheckConver][JsonParamFlags] failed!");

  ret = ConvertModelToJson(FLAGS_framework, FLAGS_om, FLAGS_json);

  GE_IF_BOOL_EXEC(ret != SUCCESS, return FAILED);
  return SUCCESS;
}

Status DisplayModelInfo() {
  // No model path passed in
  if (FLAGS_om.empty()) {
    REPORT_PREDEFINED_ERR_MSG("E10004", std::vector<const char *>({"parameter"}), std::vector<const char *>({"om"}));
    GELOGE(FAILED, "[Check][Parameter]Input parameter[--om]'s value is empty!!");
    return FAILED;
  }

  // Check if the model path is valid
  if ((!FLAGS_om.empty()) && (!CheckInputPathValid(FLAGS_om, "--om"))) {
    GELOGE(FAILED, "[Check][InputPath]model file path is invalid: %s.", FLAGS_om.c_str());
    return FAILED;
  }

  if (FLAGS_framework == -1) {
    return ConvertOm(FLAGS_om.c_str(), "", false);
  }

  REPORT_PREDEFINED_ERR_MSG("E10057", std::vector<const char *>({"parameter0", "parameter1"}),
                            std::vector<const char *>({"om", "mode"}));
  GELOGE(FAILED,
         "[Check][Parameter][--mode] and [--om], if the value of [--mode] is %u,"
         "it can be used only with the [--om] parameter!",
         static_cast<uint32_t>(RunMode::DISPLAY_OM_INFO));
  return FAILED;
}
}  // namespace

Status ConvertPbtxtToJson();

Status ConvertPbtxtToJson() {
  if (FLAGS_om.empty()) {
    REPORT_PREDEFINED_ERR_MSG("E10004", std::vector<const char *>({"parameter"}), std::vector<const char *>({"om"}));
    GELOGE(FAILED, "[Check][Parameter]Input parameter[--om]'s value is empty!");
    return FAILED;
  }

  const std::string &suffix = FLAGS_om.substr(FLAGS_om.find_last_of('.') + 1);
  if (suffix != "txt") {
    static const std::string reason = "If the value of --model is " +
                                      std::to_string(static_cast<uint32_t>(RunMode::PBTXT_TO_JSON)) +
                                      ", --om only supports *.txt format.";
    REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                              std::vector<const char *>({"--om", FLAGS_om.c_str(), reason.c_str()}));
    GELOGE(FAILED, "[Check][Parameter] Invalid value for --om[%s], %s", FLAGS_om.c_str(), reason.c_str());
    return FAILED;
  }

  Status ret = GFlagUtils::CheckConverJsonParamFlags();
  if (ret != SUCCESS) {
    GELOGE(FAILED, "[CheckConver][JsonParamFlags] failed!");
    return FAILED;
  }

  ret = ConvertPbtxtToJson(FLAGS_om.c_str(), FLAGS_json.c_str());
  if (ret != SUCCESS) {
    GELOGE(FAILED, "[Convert][PbtxtToJson] fail.");
    const std::string reason = "failed to convert pbtxt file " + FLAGS_om + " to JSON.";
    REPORT_PREDEFINED_ERR_MSG("E10059", std::vector<const char *>({"stage", "reason"}),
                              std::vector<const char *>({"Convert pbtxt to JSON", reason.c_str()}));
    return FAILED;
  }

  return SUCCESS;
}

namespace {
int32_t init() {
  domi::GetContext().atc_cmdline = flgs::GetArgv();
  // set log level
  int32_t ret = CheckLogParamValidAndSetLogLevel(FLAGS_log);
  if (ret != 0) {
    return ret;
  }
  error_message::ErrMgrInit(error_message::ErrorMessageMode::INTERNAL_MODE);
  if (ret != 0) {
    DOMI_LOGE("ErrorManager init fail !");
    return ret;
  }

  return 0;
}

long GetMemInfo(const std::string &key) {
  std::string file_path = "/proc/meminfo";
  std::ifstream fs(file_path, std::ifstream::in);
  if (!fs.is_open()) {
    GELOGW("Cannot open %s .", file_path.c_str());
    return 0;
  }
  std::string line;
  while (getline(fs, line)) {  // line not with \n
    if (line.find(key) != std::string::npos) {
      GELOGI("Find mem [%s] info line [%s]", key.c_str(), line.c_str());
      fs.close();
      size_t pos = line.find(":");
      if (pos == std::string::npos) {
        return 0;
      }
      std::string current_mem_info_str = line.substr(pos + 1);
      StringUtils::Trim(current_mem_info_str);
      GELOGI("Find mem [%s] info [%s].", key.c_str(), current_mem_info_str.c_str());
      return stol(current_mem_info_str);
    }
  }
  fs.close();  // close the file
  return 0;
}

Status CheckAndRunSingleOp() {
  if ((FLAGS_display_model_info == "1") || (FLAGS_framework != -1) || (!FLAGS_insert_op_conf.empty()) ||
      (FLAGS_mode != static_cast<int32_t>(RunMode::GEN_OM_MODEL))) {
    std::string reason(
        "When --singleop is specified, --display_model_info=1, --framework, --insert_op_conf, "
        "and --mode values other than 0 are not supported.");
    REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                              std::vector<const char *>({"--singleop", FLAGS_singleop.c_str(), reason.c_str()}));
    GELOGE(FAILED, "[Check][Parameter]%s", reason.c_str());
    return FAILED;
  }
  return GenerateSingleOp(FLAGS_singleop);
}

int32_t OutputErrMessageToStdout() {
  auto msg_ptr = error_message::GetErrMgrErrorMessage();
  if (msg_ptr == nullptr) {
    std::stringstream err_stream;
    err_stream << "E19999: Inner Error!" << std::endl;
    err_stream << "        " << "An unknown error occurred. Please check the log." << std::endl;
    std::cout << err_stream.str() << std::endl;
  } else {
    std::cout << msg_ptr.get() << std::endl;
  }
  return 0;
}

Status CheckRet(Status ret) {
  static std::map<int32_t, string> flags_mode_info_map = {
      {RunMode::GEN_OM_MODEL, "ATC generate offline model "},
      {RunMode::MODEL_TO_JSON, "ATC convert model to json file "},
      {RunMode::ONLY_PRE_CHECK, "ATC precheck "},
      {RunMode::PBTXT_TO_JSON, "ATC convert pbtxt to json file "},
      {RunMode::GEN_EXE_OM, "ATC generate execute-om "},
      {RunMode::MODEL_TO_EXE_OM, "ATC convert model to execute-om "},
      {RunMode::GEN_EXE_OM_FOR_NANO, "ATC generate execute-om for nano "},
      {RunMode::GEN_OM2_MODEL, "ATC generate OM2 model "},
  };
  string info = "";
  if (flags_mode_info_map.find(FLAGS_mode) != flags_mode_info_map.end()) {
    info += flags_mode_info_map[FLAGS_mode];
  }
  if (ret != SUCCESS) {
    GELOGW("%s", (info + "failed.").c_str());
    std::cout << "ATC run failed, Please check the detail log, Try \'" << g_usage_command
              << " --help\' for more information" << std::endl;
    int32_t result = OutputErrMessageToStdout();
    if (result != 0) {
      DOMI_LOGE("ErrorManager outputErrMessage fail !");
    }
    GELOGI("Current available mem is [%lu kB]", GetMemInfo("MemAvailable"));
    return ret;
  } else {
    GELOGI("%s", (info + "success.").c_str());
    std::cout << "ATC run success, welcome to the next use." << std::endl;
    const auto res = error_message::GetErrMgrWarningMessage();
    if (res != nullptr) {
      std::cout << res.get() << std::endl;
    }
    return 0;
  }
}

Status UpdateCheckReportPath() {
  std::string ascend_work_path;
  if ((flgs::GetUserOptions().find("check_report") == flgs::GetUserOptions().end()) &&
      (GetRawAppliedFlagNames().find("check_report") == GetRawAppliedFlagNames().end())) {
    GE_ASSERT_SUCCESS(GetAscendWorkPath(ascend_work_path));
    if (!ascend_work_path.empty()) {
      FLAGS_check_report = ascend_work_path + "/" + FLAGS_check_report;
      GELOGD("Current check report path is: %s", FLAGS_check_report.c_str());
    }
  }
  return SUCCESS;
}

Status LoadRawOptionsForAtc(std::map<std::string, std::string> &raw_options) {
  GetRawAppliedFlagNames().clear();
  GetRawAppliedFlagOptions().clear();
  GE_ASSERT_SUCCESS(LoadRawGeOptions(raw_options), "[Load][RawGeOptions]failed.");
  GE_ASSERT_SUCCESS(ApplyRawGeOptionsToFlags(raw_options), "[Apply][RawGeOptionsToFlags]failed.");
  return SUCCESS;
}

Status CheckDeprecatedAutoTuneMode() {
  if (FLAGS_auto_tune_mode.empty()) {
    return SUCCESS;
  }
  std::string reason("The Auto Tune function has been deprecated. Please use the AOE tool for tuning.");
  REPORT_PREDEFINED_ERR_MSG(
      "E10001", std::vector<const char *>({"parameter", "value", "reason"}),
      std::vector<const char *>({"--auto_tune_mode", FLAGS_auto_tune_mode.c_str(), reason.c_str()}));
  GELOGE(FAILED, "[Check][Parameter]%s", reason.c_str());
  return FAILED;
}

Status CheckInputHintShapeUnsupported() {
  if (FLAGS_input_hint_shape.empty()) {
    return SUCCESS;
  }
  const std::string reason =
      "Option[input_hint_shape: " + FLAGS_input_hint_shape + "] is not supported in ATC. Please do not set it.";
  REPORT_PREDEFINED_ERR_MSG("E10055", std::vector({"reason"}), std::vector({reason.c_str()}));
  GELOGE(FAILED, "[Check][Param] %s", reason.c_str());
  return FAILED;
}

Status CheckGlobalOptionsBeforeRun() {
  GE_ASSERT_SUCCESS(CheckDeprecatedAutoTuneMode(), "[Check][AutoTuneMode]failed.");
  GE_ASSERT_SUCCESS(CheckInputHintShapeUnsupported(), "[Check][InputHintShape]failed.");
  return SUCCESS;
}

bool IsGenerateOmMode() {
  return (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_OM_MODEL)) ||
         (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_EXE_OM)) ||
         (FLAGS_mode == static_cast<int32_t>(RunMode::ONLY_PRE_CHECK)) ||
         (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_EXE_OM_FOR_NANO)) ||
         (FLAGS_mode == static_cast<int32_t>(RunMode::GEN_OM2_MODEL));
}

Status ReportInvalidRunMode() {
  REPORT_PREDEFINED_ERR_MSG("E10001", std::vector<const char *>({"parameter", "value", "reason"}),
                            std::vector<const char *>({"--mode", std::to_string(FLAGS_mode).c_str(), kModeSupport}));
  GELOGE(PARAM_INVALID, "[Check][Parameter]Invalid value for --mode[%d], %s.", FLAGS_mode, kModeSupport);
  return FAILED;
}

Status RunAtcByMode(const std::map<std::string, std::string> &raw_options) {
  // 备份并清空注册信息map
  OperatorFactoryImpl::BackupAndClearRegInfoOnce();
  if (!FLAGS_singleop.empty()) {
    return CheckAndRunSingleOp();
  }
  GE_CHK_BOOL_EXEC(GFlagUtils::CheckWeightAndFrameWork() && GFlagUtils::CheckSocVersionAndRunmode(), return FAILED,
                   "[Check][WeightFrameworkRunmode]failed.");
  GE_ASSERT_SUCCESS(UpdateCheckReportPath(), "[Update][CheckReportPath]failed.");
  if (IsGenerateOmMode()) {
    return GenerateOmModel(raw_options);
  }
  if (FLAGS_mode == static_cast<int32_t>(RunMode::MODEL_TO_JSON)) {
    GE_ASSERT_SUCCESS(ConvertModelToJson(), "[Convert][ModelToJson]ATC ConvertJson execute failed!!");
    return SUCCESS;
  }
  if (FLAGS_mode == RunMode::PBTXT_TO_JSON) {
    GE_ASSERT_SUCCESS(ConvertPbtxtToJson(), "[Convert][PbtxtToJson]ATC convert pbtxt to json execute failed!!");
    return SUCCESS;
  }
  if (FLAGS_mode == RunMode::DISPLAY_OM_INFO) {
    GE_ASSERT_SUCCESS(DisplayModelInfo(), "[Display][ModelInfo]ATC DisplayModelInfo failed!!");
    return SUCCESS;
  }
  return ReportInvalidRunMode();
}
}  // namespace

int32_t main_impl(int32_t argc, char *argv[]) {
  Status ret = SUCCESS;
  std::cout << "ATC start working now, please wait for a moment." << std::endl;
  // Initialize
  const flgs::GfStatus flag = GFlagUtils::InitGFlag(argc, argv);
  if (flag == flgs::GF_HELP) {
    return 0;
  }
  if ((flag != flgs::GF_SUCCESS) || (init() != 0)) {
    return static_cast<int32_t>(CheckRet(-1));
  }
  std::map<std::string, std::string> raw_options;
  if (LoadRawOptionsForAtc(raw_options) != SUCCESS) {
    return static_cast<int32_t>(CheckRet(-1));
  }
  ret = (CheckGlobalOptionsBeforeRun() == SUCCESS && RunAtcByMode(raw_options) == SUCCESS) ? SUCCESS : FAILED;

  // Tbe may print ... without Enter when some op compile slow, here atc add "...\n" to display better at last
  std::cout << "..." << std::endl;
  return static_cast<int32_t>(CheckRet(ret));
} /*lint +e530*/
}  // namespace ge
