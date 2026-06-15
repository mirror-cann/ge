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
#include <limits>
#include <regex>
#include <sstream>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "mmpa/mmpa_api.h"
#include "common/ge_common/debug/ge_log.h"
#include "common/checker.h"
#include "framework/common/scope_guard.h"

namespace ge {
namespace {
struct MemFdFile {
  int32_t fd = -1;
  std::string file_name;
  std::string fd_path;
};

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

Status CreateMemFdFile(const std::string &name, const std::string &data, MemFdFile &file) {
  file.fd = static_cast<int32_t>(syscall(__NR_memfd_create, name.c_str(), 0));
  GE_ASSERT_TRUE(file.fd >= 0, "[OM2] memfd_create failed for %s", name.c_str());
  GE_DISMISSABLE_GUARD(memfd_file_cleanup, [&file]() { CloseMemFdFile(file); });

  if (!data.empty()) {
    GE_ASSERT_TRUE(data.size() <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()),
                   "[OM2] memfd data is too large, name=%s, size=%zu", name.c_str(), data.size());
    const auto write_count = mmWrite(file.fd, const_cast<char_t *>(data.data()),
                                     static_cast<uint32_t>(data.size()));
    GE_ASSERT_TRUE(write_count == static_cast<int64_t>(data.size()),
                   "[OM2] Failed to write memfd %s, expected=%zu, actual=%" PRId64, name.c_str(),
                   static_cast<int64_t>(data.size()), static_cast<int64_t>(write_count));
  }
  (void)lseek(file.fd, 0, SEEK_SET);
  file.file_name = name;
  file.fd_path = "/proc/self/fd/" + std::to_string(file.fd);
  GE_DISMISS_GUARD(memfd_file_cleanup);
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

Status ReadFdToString(const int32_t fd, std::string &data) {
  data.clear();
  (void)lseek(fd, 0, SEEK_SET);
  constexpr size_t kChunkSize = 64U * 1024U;
  std::vector<char_t> buffer(kChunkSize);
  while (true) {
    const auto read_size = read(fd, buffer.data(), buffer.size());
    GE_ASSERT_TRUE(read_size >= 0, "[OM2] Failed to read fd %d", fd);
    if (read_size == 0) {
      break;
    }
    (void)data.append(buffer.data(), static_cast<size_t>(read_size));
  }
  return SUCCESS;
}

Status CheckSafePath(const std::string &path) {
  static const std::regex kSafePathRegex(R"(^(?!.*\.{2})[A-Za-z0-9./+\-_]+$)");
  GE_ASSERT_TRUE(std::regex_match(path, kSafePathRegex), "[OM2] Unsafe compile path: %s", path.c_str());
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

Status ReplaceMakefileVariable(std::string &makefile_data, const std::string &variable_name,
                               const std::string &value) {
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

Status BuildCompileMakefileData(const Om2CodegenArtifact &makefile_artifact, const std::string &so_fd_path,
                                const std::vector<MemFdFile> &cpp_files, std::string &compile_makefile_data) {
  GE_ASSERT_SUCCESS(CheckSafePath(so_fd_path));
  for (const auto &cpp_file : cpp_files) {
    GE_ASSERT_SUCCESS(CheckSafePath(cpp_file.fd_path));
  }

  compile_makefile_data = makefile_artifact.data;
  GE_ASSERT_SUCCESS(ReplaceMakefileVariable(compile_makefile_data, "TARGET", so_fd_path));
  GE_ASSERT_SUCCESS(ReplaceMakefileVariable(compile_makefile_data, "SRC_FILES", JoinFdPaths(cpp_files)));
  // 仅用于内存编译：fd 路径没有 .cpp 后缀，且 so fd 在 make 前已经存在。
  compile_makefile_data += "\nCXXFLAGS += -x c++\n";
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
                                const bool is_release, Om2CodegenArtifact &so_artifact, MemFdFile &so_file,
                                MemFdFile &makefile_file) {
  GE_ASSERT_SUCCESS(CreateMemFdFile(so_artifact.file_name, "", so_file));

  std::string compile_makefile_data;
  GE_ASSERT_SUCCESS(BuildCompileMakefileData(makefile_artifact, so_file.fd_path, cpp_files, compile_makefile_data));
  GE_ASSERT_SUCCESS(CreateMemFdFile("Makefile", compile_makefile_data, makefile_file));
  GE_ASSERT_SUCCESS(CheckSafePath(makefile_file.fd_path));

  const char_t *const use_stub_lib = is_release ? "0" : "1";
  std::ostringstream oss;
  oss << "make -s -f " << makefile_file.fd_path << " USE_STUB_LIB=" << use_stub_lib;
  const std::string command = oss.str();
  GELOGI("[OM2] Compile generated cpp artifacts to so by Makefile, command: %s", command.c_str());
  GE_CHK_BOOL_RET_STATUS(system(command.c_str()) == 0, FAILED, "[OM2] Failed to compile so artifact: %s",
                         so_artifact.file_name.c_str());
  GE_ASSERT_SUCCESS(ReadFdToString(so_file.fd, so_artifact.data));
  GE_ASSERT_TRUE(!so_artifact.data.empty(), "[OM2] Compiled so artifact is empty: %s", so_artifact.file_name.c_str());
  return SUCCESS;
}

}  // namespace
Status Om2Utils::CompileGeneratedCppToSo(const Om2CodegenArtifacts &artifacts, const std::string &model_name,
                                         Om2CodegenArtifact &so_artifact, const bool is_release) {
  const std::string interface_name = model_name + "_interface.h";
  const Om2CodegenArtifact *interface_artifact = nullptr;
  GE_ASSERT_SUCCESS(FindArtifact(artifacts, interface_name, interface_artifact));
  const Om2CodegenArtifact *makefile_artifact = nullptr;
  GE_ASSERT_SUCCESS(FindArtifact(artifacts, "Makefile", makefile_artifact));

  MemFdFile header_file;
  std::vector<MemFdFile> cpp_files;
  MemFdFile so_file;
  MemFdFile makefile_file;
  auto memfd_cleanup_callback = [&header_file, &cpp_files, &so_file, &makefile_file]() {
    CloseMemFdFile(header_file);
    CloseMemFdFiles(cpp_files);
    CloseMemFdFile(so_file);
    CloseMemFdFile(makefile_file);
  };
  GE_MAKE_GUARD(memfd_cleanup, memfd_cleanup_callback);
  GE_ASSERT_SUCCESS(CreateMemFdFile(interface_name, interface_artifact->data, header_file));
  GE_ASSERT_SUCCESS(CreateCompileCppFiles(artifacts, interface_name, header_file.fd_path, cpp_files));
  GE_CHK_BOOL_RET_STATUS(!cpp_files.empty(), FAILED, "[OM2] No generated cpp artifacts found for model %s",
                         model_name.c_str());

  so_artifact.file_name = "lib" + model_name + "_om2.so";
  GE_ASSERT_SUCCESS(CompileWithMemFdMakefile(*makefile_artifact, cpp_files, is_release, so_artifact, so_file,
                                             makefile_file));
  return GRAPH_SUCCESS;
}

}  // namespace ge
