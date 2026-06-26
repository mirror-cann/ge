/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "om2_utils.h"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include <sys/utsname.h>
#include "mmpa/mmpa_api.h"
#include "common/ge_common/ge_types.h"
#include "common/ge_common/debug/ge_log.h"
#include "common/checker.h"
#include "framework/common/scope_guard.h"
#include "graph/ge_context.h"

namespace ge {
namespace {
constexpr const char_t *kOptionBuildConfig = "ge.buildConfig";

struct MemFdFile {
  int32_t fd = -1;
  std::string file_name;
  std::string fd_path;
};

Status CheckSafePath(const std::string &path) {
  static const std::regex kSafePathRegex(R"(^(?!.*\.{2})[A-Za-z0-9./+\-_]+$)");
  GE_ASSERT_TRUE(std::regex_match(path, kSafePathRegex), "[OM2] Unsafe compile path: %s", path.c_str());
  return SUCCESS;
}

void CloseMemFdFile(MemFdFile &file) {
  if (file.fd >= 0) {
    (void)close(file.fd);
    file.fd = -1;
  }
}

void CloseMemFdFiles(std::vector<MemFdFile> &files) {
  for (auto &file : files) {
    CloseMemFdFile(file);
  }
}

void RemoveTempFile(std::string &path) {
  if (!path.empty()) {
    (void)unlink(path.c_str());
    path.clear();
  }
}

Status CreateMemFdFile(const std::string &name, const std::string &data, MemFdFile &file) {
  file.fd = static_cast<int32_t>(syscall(__NR_memfd_create, name.c_str(), 0));
  GE_ASSERT_TRUE(file.fd >= 0, "[OM2] memfd_create failed for %s", name.c_str());
  GE_DISMISSABLE_GUARD(memfd_file_cleanup, [&file]() { CloseMemFdFile(file); });

  if (!data.empty()) {
    GE_ASSERT_TRUE(data.size() <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()),
                   "[OM2] memfd data is too large, name=%s, size=%zu", name.c_str(), data.size());
    const auto write_count = mmWrite(file.fd, const_cast<char_t *>(data.data()), static_cast<uint32_t>(data.size()));
    GE_ASSERT_TRUE(write_count == static_cast<int64_t>(data.size()),
                   "[OM2] Failed to write memfd %s, expected=%zu, actual=%" PRId64, name.c_str(),
                   static_cast<int64_t>(data.size()), static_cast<int64_t>(write_count));
  }
  (void)lseek(file.fd, 0, SEEK_SET);
  file.file_name = name;
  file.fd_path = "/proc/" + std::to_string(getpid()) + "/fd/" + std::to_string(file.fd);
  GE_DISMISS_GUARD(memfd_file_cleanup);
  return SUCCESS;
}

Status CreateTempFile(const std::string &name, std::string &file_path) {
  std::string path_template = "/tmp/.om2_" + std::to_string(getpid()) + "_" + name + "_XXXXXX";
  GE_ASSERT_SUCCESS(CheckSafePath(path_template));
  std::vector<char_t> path(path_template.begin(), path_template.end());
  path.push_back('\0');
  const int32_t fd = mkstemp(path.data());
  GE_ASSERT_TRUE(fd >= 0, "[OM2] Failed to create temp file for %s", name.c_str());
  (void)close(fd);
  file_path = path.data();
  GE_ASSERT_SUCCESS(CheckSafePath(file_path));
  return SUCCESS;
}

bool IsCppFile(const std::string &file_name) {
  constexpr const char_t *kCppSuffix = ".cpp";
  constexpr size_t kCppSuffixSize = 4U;
  return (file_name.size() >= kCppSuffixSize) &&
         (file_name.compare(file_name.size() - kCppSuffixSize, kCppSuffixSize, kCppSuffix) == 0);
}

Status FindArtifact(const Om2CodegenArtifacts &artifacts, const std::string &file_name,
                    const Om2CodegenArtifact *&artifact) {
  artifact = nullptr;
  for (const auto &item : artifacts) {
    if (item.file_name == file_name) {
      artifact = &item;
      return SUCCESS;
    }
  }
  GELOGE(FAILED, "[OM2] Artifact %s not found", file_name.c_str());
  return FAILED;
}

Status ReadFileToString(const std::string &path, std::string &data) {
  data.clear();
  GE_ASSERT_SUCCESS(CheckSafePath(path));
  std::ifstream input(path, std::ios::in | std::ios::binary);
  GE_ASSERT_TRUE(input.is_open(), "[OM2] Failed to open file: %s", path.c_str());
  constexpr size_t kChunkSize = 64U * 1024U;
  std::vector<char_t> buffer(kChunkSize);
  while (true) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto read_size = input.gcount();
    if (read_size > 0) {
      data.append(buffer.data(), static_cast<size_t>(read_size));
    }
    if (input.eof()) {
      break;
    }
    GE_ASSERT_TRUE(input.good(), "[OM2] Failed to read file: %s", path.c_str());
  }
  return SUCCESS;
}

std::string JoinFdPaths(const std::vector<MemFdFile> &files) {
  std::ostringstream oss;
  bool is_first = true;
  for (const auto &file : files) {
    if (!is_first) {
      oss << ' ';
    }
    oss << file.fd_path;
    is_first = false;
  }
  return oss.str();
}

Status CheckBuildConfigValue(const std::string &value) {
  constexpr std::string_view kAllowedChars = " _-./+,:=@%'\"";
  for (const auto c : value) {
    // Keep build_config as make arguments instead of opening a general shell command surface.
    const bool is_allowed = std::isalnum(static_cast<uint8_t>(c)) || (kAllowedChars.find(c) != std::string_view::npos);
    GE_ASSERT_TRUE(is_allowed, "[OM2] Invalid character in build_config");
  }
  return SUCCESS;
}

Status SplitCommandTokens(const std::string &command, std::vector<std::string> &tokens) {
  tokens.clear();
  std::string token;
  // 引号感知拆分：避免 CXXFLAGS="-O2 -fPIC" 中的 "-fPIC" 被误判为 make 的 "-f"（Makefile）选项
  char_t quote = '\0';  // '\0' 表示引号外；'\'' 或 '"' 表示在对应引号内
  for (const auto c : command) {
    if (quote != '\0') {
      // 在引号内：持续消费字符，直到遇到匹配的闭合引号
      token.push_back(c);
      if (c == quote) {
        quote = '\0';
      }
      continue;
    }
    if ((c == '\'') || (c == '"')) {
      // 进入引号区域：记录开启的引号类型
      quote = c;
      token.push_back(c);
      continue;
    }
    if (std::isspace(static_cast<uint8_t>(c))) {
      // 未加引号的空白字符：输出当前 token 作为分隔边界
      if (!token.empty()) {
        tokens.emplace_back(token);
        token.clear();
      }
      continue;
    }
    // 普通未加引号字符：追加到当前 token
    token.push_back(c);
  }
  // 校验引号闭合
  GE_ASSERT_TRUE(quote == '\0', "[OM2] Unbalanced quote in build_config");
  if (!token.empty()) {
    tokens.emplace_back(token);
  }
  return SUCCESS;
}

// 支持 "make" 和 "/usr/bin/make" 等绝对路径形式，防止 build_config 经由 system() 执行任意命令
bool IsMakeCommand(const std::string &first_token) {
  if (first_token == "make") {
    return true;
  }
  constexpr std::string_view kMakeSuffix = "/make";
  if (first_token.size() < kMakeSuffix.size()) {
    return false;
  }
  return first_token.compare(first_token.size() - kMakeSuffix.size(), kMakeSuffix.size(), kMakeSuffix) == 0;
}

bool IsMakefileOption(const std::string &token) {
  return (token == "-f") || (token.rfind("-f", 0U) == 0U) || (token == "--file") ||
         (token.rfind("--file=", 0U) == 0U) || (token == "--makefile") || (token.rfind("--makefile=", 0U) == 0U);
}

bool IsAllowedBuildConfigVariable(const std::string &key) {
  static const std::set<std::string> kAllowedVariables = {"CXX", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "LDLIBS"};
  return kAllowedVariables.count(key) > 0;
}

Status AddGeneratedMakefileOption(const std::string &makefile_path, std::string &command) {
  std::vector<std::string> tokens;
  GE_ASSERT_SUCCESS(SplitCommandTokens(command, tokens));
  for (auto iter = tokens.begin() + 1; iter != tokens.end(); ++iter) {
    GE_ASSERT_TRUE(!IsMakefileOption(*iter), "[OM2] build_config does not support custom Makefile option");
    // make flag（-j, -s 等）放行
    if (!iter->empty() && (*iter)[0] == '-') {
      continue;
    }
    // 变量赋值：只允许白名单内的变量名
    const auto eq_pos = iter->find('=');
    GE_ASSERT_TRUE(eq_pos != std::string::npos, "[OM2] build_config: unexpected token '%s'", iter->c_str());
    const std::string key = iter->substr(0, eq_pos);
    GE_ASSERT_TRUE(IsAllowedBuildConfigVariable(key),
                   "[OM2] build_config: variable '%s' is not allowed, "
                   "only CXX, CXXFLAGS, CPPFLAGS, LDFLAGS, LDLIBS are supported",
                   key.c_str());
  }
  // The generated Makefile lives on a memfd path, so GE must inject it instead of relying on cwd lookup.
  // command 已通过 CheckBuildConfigValue 校验，白名单中唯一允许的空白字符是空格，不含 tab，因此用 find(' ') 即可
  const auto first_space = command.find(' ');
  command.insert(first_space, " -f " + makefile_path);
  return SUCCESS;
}

std::string BuildDefaultMakeCommand(const std::string &makefile_path) {
  // 剥离可能污染 Makefile 默认值的环境变量，只影响 make 子进程，不影响父进程环境
  return "env -u CXX -u CXXFLAGS -u CPPFLAGS -u LDFLAGS -u LDLIBS make -s -f " + makefile_path;
}

std::string GetCurrentHostCpu() {
  struct utsname uts;
  if (uname(&uts) != 0) {
    return "";
  }
  return std::string(uts.machine);
}

bool IsArmCpu(const std::string &cpu) {
  return Om2Utils::NormalizeCpuArch(cpu) == "aarch64";
}

std::string FindAarch64LinuxCrossCompiler() {
  // 优先级1: 系统 PATH（标准命名）
  constexpr const char_t *kSystemCompilerName = "aarch64-linux-gnu-g++";
  const char_t *path_env = std::getenv("PATH");
  if (path_env != nullptr) {
    std::istringstream path_stream(path_env);
    std::string dir;
    while (std::getline(path_stream, dir, ':')) {
      const std::string candidate = dir + "/" + kSystemCompilerName;
      if (access(candidate.c_str(), X_OK) == 0) {
        GELOGI("[OM2] Found system cross-compiler: %s", candidate.c_str());
        return candidate;
      }
    }
  }

  // 优先级2: CANN 包
  constexpr const char_t *kCannCompilerName = "aarch64-target-linux-gnu-g++";
  const char_t *ascend_home = nullptr;
  MM_SYS_GET_ENV(MM_ENV_ASCEND_HOME_PATH, ascend_home);
  if (ascend_home != nullptr) {
    const std::string cann_compiler = std::string(ascend_home) + "/tools/hcc/bin/" + kCannCompilerName;
    if (access(cann_compiler.c_str(), X_OK) == 0) {
      GELOGI("[OM2] Found CANN cross-compiler: %s", cann_compiler.c_str());
      return cann_compiler;
    }
  }

  GELOGD("[OM2] aarch64-linux cross-compiler not found in system PATH or CANN package.");
  return "";
}

std::string FindAarch64Devlib() {
  const char_t *ascend_home = nullptr;
  MM_SYS_GET_ENV(MM_ENV_ASCEND_HOME_PATH, ascend_home);
  if (ascend_home == nullptr) {
    return "";
  }

  const std::string devlib_path = std::string(ascend_home) + "/devlib/linux/aarch64";
  if (access(devlib_path.c_str(), F_OK) == 0) {
    GELOGI("[OM2] Found aarch64 devlib: %s", devlib_path.c_str());
    return devlib_path;
  }

  GELOGD("[OM2] aarch64 devlib not found.");
  return "";
}

Status AppendCrossCompilerIfNeeded(std::string &command) {
  std::string host_env_os;
  std::string host_env_cpu;
  const bool has_os = (ge::GetContext().GetOption(OPTION_HOST_ENV_OS, host_env_os) == GRAPH_SUCCESS);
  const bool has_cpu = (ge::GetContext().GetOption(OPTION_HOST_ENV_CPU, host_env_cpu) == GRAPH_SUCCESS);
  // 两个都必须传入才触发交叉编译
  if (!has_os || !has_cpu || host_env_os.empty() || host_env_cpu.empty()) {
    return SUCCESS;
  }
  // 获取当前架构
  const std::string cur_cpu = Om2Utils::NormalizeCpuArch(GetCurrentHostCpu());
  const std::string target_cpu = Om2Utils::NormalizeCpuArch(host_env_cpu);
  // 如果目标与当前架构一致，无需交叉编译
  if (target_cpu == cur_cpu) {
    GELOGI("[OM2] Target CPU %s matches host CPU, skip cross-compiler injection", host_env_cpu.c_str());
    return SUCCESS;
  }
  // 目标与当前架构不一致，检查是否是 ARM 目标
  if (!IsArmCpu(target_cpu)) {
    GELOGW("[OM2] Unexpected non-ARM target for cross-compile: os=%s, cpu=%s", host_env_os.c_str(),
           host_env_cpu.c_str());
    return SUCCESS;
  }
  // 目标是 ARM 且与当前架构不同，查找交叉编译器
  const std::string compiler = FindAarch64LinuxCrossCompiler();
  if (compiler.empty()) {
    GELOGE(FAILED,
           "[OM2] aarch64-linux cross-compiler not found. "
           "Please install gcc-aarch64-linux-gnu or ensure ASCEND_HOME_PATH is set correctly.");
    return FAILED;
  }
  // 注入 CXX
  command += " CXX=" + compiler;
  GELOGI("[OM2] Cross-compiler injected: CXX=%s", compiler.c_str());
  // 查找并注入 LIB_PATH
  const std::string devlib = FindAarch64Devlib();
  if (devlib.empty()) {
    GELOGE(FAILED,
           "[OM2] aarch64 devlib not found. "
           "Please ensure ASCEND_HOME_PATH is set correctly and devlib/linux/aarch64 exists.");
    return FAILED;
  }
  command += " LIB_PATH=" + devlib;
  GELOGI("[OM2] LIB_PATH overridden for cross-compilation: %s", devlib.c_str());

  return SUCCESS;
}

Status BuildMakeCommand(const std::string &makefile_path, const bool is_release, std::string &command) {
  // 读取用户自定义编译命令
  std::string build_config;
  const bool has_build_config = ge::GetContext().GetOption(kOptionBuildConfig, build_config) == GRAPH_SUCCESS;

  // 空白字符只允许空格（由 CheckBuildConfigValue 限定），不含 tab
  const auto first_non_space = build_config.find_first_not_of(' ');
  if ((!has_build_config) || (first_non_space == std::string::npos)) {
    // 默认路径：构建 make 命令，交叉编译时自动注入 CXX 和 LIB_PATH
    command = BuildDefaultMakeCommand(makefile_path);
    GE_ASSERT_SUCCESS(AppendCrossCompilerIfNeeded(command));
  } else {
    // 自定义路径：校验字符集、首 token 必须是 make、变量名在白名单内，并注入 -f
    build_config.erase(0U, first_non_space);
    GE_ASSERT_SUCCESS(CheckBuildConfigValue(build_config));
    const auto first_space = build_config.find(' ');
    const std::string first_token = build_config.substr(0, first_space);
    GE_ASSERT_TRUE(IsMakeCommand(first_token), "[OM2] build_config must start with make command");
    GE_ASSERT_SUCCESS(AddGeneratedMakefileOption(makefile_path, build_config));
    command = build_config;
  }

  // 始终注入 USE_STUB_LIB，控制链接桩库还是真实库
  command += " USE_STUB_LIB=";
  command += is_release ? "0" : "1";
  return SUCCESS;
}

bool IsMakefileLineContinued(const std::string &data, const size_t line_begin, const size_t line_end) {
  size_t pos = line_end;
  while ((pos > line_begin) && (std::isspace(static_cast<uint8_t>(data[pos - 1U])) != 0)) {
    --pos;
  }
  return (pos > line_begin) && (data[pos - 1U] == '\\');
}

size_t FindMakefileVariableReplaceEnd(const std::string &data, size_t line_begin) {
  size_t line_end = data.find('\n', line_begin);
  while ((line_end != std::string::npos) && IsMakefileLineContinued(data, line_begin, line_end)) {
    line_begin = line_end + 1U;
    line_end = data.find('\n', line_begin);
  }
  return line_end;
}

Status ReplaceMakefileVariable(std::string &makefile_data, const std::string &variable_name, const std::string &value) {
  const std::string variable_prefix = variable_name + " :=";
  size_t line_begin = 0U;
  while (line_begin < makefile_data.size()) {
    const bool is_target_line = (makefile_data.compare(line_begin, variable_prefix.size(), variable_prefix) == 0);
    const size_t line_end = makefile_data.find('\n', line_begin);
    if (is_target_line) {
      const size_t value_begin = line_begin + variable_prefix.size();
      const size_t replace_end = FindMakefileVariableReplaceEnd(makefile_data, line_begin);
      const size_t value_size = (replace_end == std::string::npos) ? std::string::npos : (replace_end - value_begin);
      (void)makefile_data.replace(value_begin, value_size, " " + value);
      return SUCCESS;
    }
    if (line_end == std::string::npos) {
      break;
    }
    line_begin = line_end + 1U;
  }
  GELOGE(FAILED, "[OM2] Makefile variable %s not found", variable_name.c_str());
  return FAILED;
}

Status BuildCompileMakefileData(const Om2CodegenArtifact &makefile_artifact, const std::string &so_path,
                                const std::vector<MemFdFile> &cpp_files, std::string &compile_makefile_data) {
  GE_ASSERT_SUCCESS(CheckSafePath(so_path));
  for (const auto &cpp_file : cpp_files) {
    GE_ASSERT_SUCCESS(CheckSafePath(cpp_file.fd_path));
  }

  compile_makefile_data = makefile_artifact.data;
  GE_ASSERT_SUCCESS(ReplaceMakefileVariable(compile_makefile_data, "TARGET", so_path));
  GE_ASSERT_SUCCESS(ReplaceMakefileVariable(compile_makefile_data, "SRC_FILES", JoinFdPaths(cpp_files)));
  // 仅用于内存编译：fd 路径没有 .cpp 后缀，需要 -x c++ 显式指定语言。
  // 用户通过 --build_config 传 CXXFLAGS 时完全覆盖默认值，此处确保 -x c++ 不被遗漏。
  compile_makefile_data += R"(
ifndef CXXFLAGS
CXXFLAGS := -x c++
endif
ifeq ($(filter -x c++,$(CXXFLAGS)),)
override CXXFLAGS += -x c++
endif
)";
  compile_makefile_data += ".PHONY: $(TARGET)\n";
  GE_ASSERT_TRUE(compile_makefile_data.find("TARGET := lib") == std::string::npos,
                 "[OM2] Compile Makefile target still points to package so name");
  return SUCCESS;
}

Status CreateCompileCppFiles(const Om2CodegenArtifacts &artifacts, const std::string &interface_name,
                             const std::string &header_fd_path, std::vector<MemFdFile> &cpp_files) {
  const std::string old_include = "#include \"" + interface_name + "\"";
  const std::string new_include = "#include \"" + header_fd_path + "\"";
  for (const auto &artifact : artifacts) {
    if (!IsCppFile(artifact.file_name)) {
      continue;
    }
    std::string compile_data = artifact.data;
    size_t pos = 0U;
    bool replaced = false;
    pos = compile_data.find(old_include, pos);
    while (pos != std::string::npos) {
      (void)compile_data.replace(pos, old_include.size(), new_include);
      pos += new_include.size();
      replaced = true;
      pos = compile_data.find(old_include, pos);
    }
    GE_ASSERT_TRUE(replaced, "[OM2] Interface include %s not found in %s", interface_name.c_str(),
                   artifact.file_name.c_str());

    MemFdFile cpp_file;
    GE_ASSERT_SUCCESS(CreateMemFdFile(artifact.file_name, compile_data, cpp_file));
    cpp_files.push_back(std::move(cpp_file));
  }
  return SUCCESS;
}

Status CompileWithMemFdMakefile(const Om2CodegenArtifact &makefile_artifact, const std::vector<MemFdFile> &cpp_files,
                                const bool is_release, Om2CodegenArtifact &so_artifact, std::string &so_path,
                                MemFdFile &makefile_file) {
  GE_ASSERT_SUCCESS(CreateTempFile(so_artifact.file_name, so_path));

  std::string compile_makefile_data;
  GE_ASSERT_SUCCESS(BuildCompileMakefileData(makefile_artifact, so_path, cpp_files, compile_makefile_data));
  GE_ASSERT_SUCCESS(CreateMemFdFile("Makefile", compile_makefile_data, makefile_file));
  GE_ASSERT_SUCCESS(CheckSafePath(makefile_file.fd_path));

  std::string command;
  GE_ASSERT_SUCCESS(BuildMakeCommand(makefile_file.fd_path, is_release, command));
  GELOGI("[OM2] Compile generated cpp artifacts to so by Makefile, command: %s", command.c_str());
  GE_CHK_BOOL_RET_STATUS(system(command.c_str()) == 0, FAILED, "[OM2] Failed to compile so artifact: %s",
                         so_artifact.file_name.c_str());
  GE_ASSERT_SUCCESS(ReadFileToString(so_path, so_artifact.data));
  GE_ASSERT_TRUE(!so_artifact.data.empty(), "[OM2] Compiled so artifact is empty: %s", so_artifact.file_name.c_str());
  return SUCCESS;
}

}  // namespace

std::string Om2Utils::NormalizeCpuArch(const std::string &cpu) {
  if (cpu == "arm64") {
    return "aarch64";
  }
  return cpu;
}

Status Om2Utils::CompileGeneratedCppToSo(const Om2CodegenArtifacts &artifacts, const std::string &model_name,
                                         Om2CodegenArtifact &so_artifact, const bool is_release) {
  const std::string interface_name = model_name + "_interface.h";
  const Om2CodegenArtifact *interface_artifact = nullptr;
  GE_ASSERT_SUCCESS(FindArtifact(artifacts, interface_name, interface_artifact));
  const Om2CodegenArtifact *makefile_artifact = nullptr;
  GE_ASSERT_SUCCESS(FindArtifact(artifacts, "Makefile", makefile_artifact));

  MemFdFile header_file;
  std::vector<MemFdFile> cpp_files;
  std::string so_path;
  MemFdFile makefile_file;
  auto memfd_cleanup_callback = [&header_file, &cpp_files, &so_path, &makefile_file]() {
    CloseMemFdFile(header_file);
    CloseMemFdFiles(cpp_files);
    RemoveTempFile(so_path);
    CloseMemFdFile(makefile_file);
  };
  GE_MAKE_GUARD(memfd_cleanup, memfd_cleanup_callback);
  GE_ASSERT_SUCCESS(CreateMemFdFile(interface_name, interface_artifact->data, header_file));
  GE_ASSERT_SUCCESS(CreateCompileCppFiles(artifacts, interface_name, header_file.fd_path, cpp_files));
  GE_CHK_BOOL_RET_STATUS(!cpp_files.empty(), FAILED, "[OM2] No generated cpp artifacts found for model %s",
                         model_name.c_str());

  so_artifact.file_name = "lib" + model_name + "_om2.so";
  GE_ASSERT_SUCCESS(
      CompileWithMemFdMakefile(*makefile_artifact, cpp_files, is_release, so_artifact, so_path, makefile_file));
  return GRAPH_SUCCESS;
}

}  // namespace ge
