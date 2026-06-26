/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_HELPER_ZIP_ARCHIVE_WRITER_H
#define BASE_COMMON_HELPER_ZIP_ARCHIVE_WRITER_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "common/ge_common/ge_types.h"
#include "minizip/ioapi.h"
#include "minizip/unzip.h"
#include "minizip/zip.h"

namespace ge {
struct ModelBufferData;

struct MemoryFile {
  uint8_t *buffer;               // Writable buffer.
  uint64_t length;               // Actual writable buffer content length.
  uint64_t capacity;             // Writable buffer capacity.
  uint64_t position;             // Current position.
  int32_t error;                 // Error flag.
  int32_t grow_mode;             // Whether the buffer can grow.
  int32_t release_from_outside;  // 0 means zipClose releases buffer, 1 means external release.
};

struct SimpleZipMemoryFileReadonly {
  const uint8_t *buffer;  // Read-only buffer managed by caller.
  uint64_t length;        // Actual read-only buffer content length.
  uint64_t position;      // Current position.
};

class SimpleZipArchiveReader {
 public:
  SimpleZipArchiveReader(const uint8_t *data, size_t length);
  ~SimpleZipArchiveReader();
  bool IsGood() const {
    return zip_handle_ != nullptr;
  }
  std::vector<std::string> ListFiles() const;
  ReadonlyByteBuffer ExtractToMem(const std::string &entry_name, size_t &buffer_size) const;

 private:
  SimpleZipMemoryFileReadonly mem_file_;
  unzFile zip_handle_ = nullptr;
};

class ZipArchiveWriter {
 public:
  explicit ZipArchiveWriter(const std::string &archive_path);
  ~ZipArchiveWriter();
  bool WriteFile(const std::string &entry_name, const std::string &src_file_path, const bool compress = true);
  /**
   * Write a memory buffer as a file entry into the opened zip archive.
   * @param entry_name  Name of the file inside the zip archive.
   * @param data      Pointer to the buffer to write. Must not be null.
   * @param data_size Size in bytes of the buffer. Extremely large (>4GB) buffers are supported.
   * @param compress  Whether to compress the data. If false, the compression method is STORED.
   * @return true on success, false if any error occurs.
   */
  bool WriteBytes(const std::string &entry_name, const void *data, const size_t data_size, const bool compress = true);
  bool SaveModelData(ModelBufferData &model, bool save_to_file);
  bool SaveModelDataToFile();
  bool IsMemFileOpened() const {
    return (zip_handle_ != nullptr) && (mem_file_.buffer != nullptr);
  }

 private:
  bool InitArchive();
  bool WriteEndOfFile();
  bool SaveModelDataToBuffer(ModelBufferData &model);

 private:
  std::string archive_path_;
  std::string base_name_;
  zipFile zip_handle_{};
  MemoryFile mem_file_{};
  std::unordered_set<std::string> files_written_;
};
}  // namespace ge

#endif  // BASE_COMMON_HELPER_ZIP_ARCHIVE_WRITER_H
