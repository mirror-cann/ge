/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RUNTIME_OM2_ZIP_ARCHIVE_READER_H
#define RUNTIME_OM2_ZIP_ARCHIVE_READER_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/ge_common/ge_types.h"
#include "minizip/unzip.h"

namespace ge {
struct MemoryFileReadonly {
  const uint8_t *buffer;  // Read-only buffer managed by caller.
  uint64_t length;        // Actual read-only buffer content length.
  uint64_t position;      // Current position.
};

class RAIIZipArchive {
 public:
  /**
   * Constructs a RAIIZipArchive object using a ZIP archive that already
   * resides in memory.
   * @param data Pointer to the beginning of the ZIP data in memory.
   * @param length Size of the ZIP data in bytes.
   */
  RAIIZipArchive(const uint8_t *data, const size_t length);

  ~RAIIZipArchive();
  bool IsGood() const {
    return (zip_handle_ != nullptr) && entry_cache_ready_;
  }
  /**
   * Lists all regular files in the ZIP archive, excluding directories.
   * @return Vector containing relative paths of all files in the archive.
   */
  std::vector<std::string> ListFiles() const;
  /**
   * Extracts a single file from the ZIP archive to the specified directory.
   * @param entry_name Filename (relative path) within the ZIP archive.
   * @param output_dir Output directory. Will be created if it does not exist.
   * @return true if extraction succeeded, false otherwise.
   */
  bool ExtractToFile(const std::string &entry_name, const std::string &output_dir) const;
  ReadonlyByteBuffer ExtractToMem(const std::string &entry_name, size_t &buff_size) const;

 private:
  struct CachedZipEntry {
    unz64_file_pos file_pos;
    uint64_t uncompressed_size = 0U;
    uint64_t raw_data_offset = 0U;
    // 仅未压缩 entry 的 raw_data_offset 可直接用于零拷贝读取。
    bool raw_data_ready = false;
  };

  bool BuildEntryCache();
  bool CacheCurrentEntry(const std::string &entry_name, const unz_file_info64 &file_info);
  bool GoToEntry(const std::string &entry_name) const;
  bool GetCachedRawData(const std::string &entry_name, size_t &buff_size, ReadonlyByteBuffer &raw_data) const;
  bool GetRawDataOffset(const size_t pos_in_central_dir, const size_t buff_size, uint64_t &raw_data_offset) const;

 private:
  MemoryFileReadonly mem_file_{};
  unzFile zip_handle_ = nullptr;
  std::unordered_map<std::string, CachedZipEntry> entry_cache_;
  std::vector<std::string> entry_names_;
  bool entry_cache_ready_ = false;
};
}  // namespace ge

#endif  // RUNTIME_OM2_ZIP_ARCHIVE_READER_H
