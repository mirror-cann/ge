/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/helper/om2/zip_archive_writer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

#include "common/checker.h"
#include "common/debug/ge_log.h"
#include "common/math/math_util.h"
#include "common/scope_guard.h"
#include "ge/ge_ir_build.h"
#include "graph_metadef/graph/utils/file_utils.h"
#include "graph_metadef/graph/utils/math_util.h"
#include "mmpa/mmpa_api.h"

namespace ge {
namespace {
constexpr int kMemZipOk = 0;
constexpr int kMemZipError = -1;
constexpr uint64_t kMemInitialCapacity = 64 * 1024;
constexpr int kMemGrowFactor = 2;
constexpr int64_t kBufSize = 16384UL;  // same as UNZ_BUFSIZE
constexpr uint32_t kMaxWriteSize = std::numeric_limits<uint32_t>::max();
constexpr uint32_t kMaxFileNameLength = 4096U;  // same as UNZ_MAXFILENAMEINZIP

int MemGrow(MemoryFile *mem_file, const uint64_t new_size) {
  if (new_size <= mem_file->capacity) {
    return kMemZipOk;
  }

  uint64_t new_capacity = mem_file->capacity;
  if (new_capacity == 0) {
    new_capacity = kMemInitialCapacity;
  }

  while (new_capacity < new_size) {
    if (CheckUint64MulOverflow(new_capacity, kMemGrowFactor) != SUCCESS) {
      GELOGE(FAILED,
             "[MEMZIP] Memory file capacity overflow: current_capacity[%lu bytes], grow_factor[%d], target_size[%zu "
             "bytes]",
             new_capacity, kMemGrowFactor, new_size);
      return kMemZipError;
    }
    new_capacity *= kMemGrowFactor;
  }

  const auto new_buffer = static_cast<uint8_t *>(std::malloc(new_capacity));
  if (new_buffer == nullptr) {
    GELOGE(FAILED, "[MEMZIP] Failed to allocate memory: target_capacity[%lu bytes], error_msg[%s]",
           new_capacity, strerror(errno));
    return kMemZipError;
  }

  if ((mem_file->buffer != nullptr) && (mem_file->length > 0)) {
    const auto ret = GeMemcpy(new_buffer, new_capacity, mem_file->buffer, mem_file->length);
    if (ret != SUCCESS) {
      GELOGE(FAILED,
             "[MEMZIP] Failed to copy memory: dest_ptr[%p], dest_max[%lu], src_ptr[%p], src_size[%lu], ret=%d",
             new_buffer, new_capacity, mem_file->buffer, mem_file->capacity, ret);
      std::free(new_buffer);
      return kMemZipError;
    }
  }

  std::free(mem_file->buffer);
  mem_file->buffer = new_buffer;
  mem_file->capacity = new_capacity;
  return kMemZipOk;
}

voidpf ZCALLBACK MemOpenFileFuncWithBuffer(voidpf opaque, const void *filename, int mode) {
  (void)filename;

  const auto mem_file = static_cast<MemoryFile *>(opaque);
  if (mem_file == nullptr) {
    return nullptr;
  }

  mem_file->buffer = nullptr;
  mem_file->length = 0;
  mem_file->capacity = 0;
  mem_file->position = 0;
  mem_file->error = kMemZipOk;
  mem_file->grow_mode = 1;
  mem_file->release_from_outside = 1;

  if ((mode & ZLIB_FILEFUNC_MODE_CREATE) != 0) {
    if (MemGrow(mem_file, kMemInitialCapacity) != kMemZipOk) {
      GELOGE(FAILED, "[MEMZIP] Failed to allocate initial capacity[%zu bytes]", kMemInitialCapacity);
      return nullptr;
    }
  }

  return mem_file;
}

template <typename T>
uLong MemReadFileImpl(T *mem_file, void *buf, const uLong size) {
  if (mem_file == nullptr) {
    return 0;
  }

  uLong bytes_to_read = size;
  if (mem_file->position + bytes_to_read > mem_file->length) {
    bytes_to_read = mem_file->length - mem_file->position;
  }

  if (bytes_to_read > 0) {
    const auto ret = GeMemcpy(static_cast<uint8_t *>(buf), size, mem_file->buffer + mem_file->position, bytes_to_read);
    if (ret != SUCCESS) {
      GELOGE(FAILED,
             "[MEMZIP] Failed to copy, ret=%d: dest_ptr[%p], dest_max[%zu], src_base_ptr[%p], src_position[%zu], "
             "src_size[%zu]",
             ret, buf, size, mem_file->buffer, mem_file->position, bytes_to_read);
      return 0;
    }
    mem_file->position += bytes_to_read;
  }

  return bytes_to_read;
}

uLong ZCALLBACK MemReadFileFunc(voidpf opaque, voidpf stream, void *buf, uLong size) {
  const auto mem_file = static_cast<MemoryFile *>(stream);
  (void)opaque;

  if (mem_file == nullptr || mem_file->error != kMemZipOk) {
    return 0;
  }
  return MemReadFileImpl(mem_file, buf, size);
}

uLong ZCALLBACK MemWriteFileFunc(voidpf opaque, voidpf stream, const void *buf, uLong size) {
  const auto mem_file = static_cast<MemoryFile *>(stream);
  (void)opaque;

  if (mem_file == nullptr || mem_file->error != kMemZipOk) {
    GELOGE(FAILED, "[MEMZIP] Opaque pointer is null or file is invalid");
    return 0;
  }

  if (size == 0) {
    return 0;
  }

  uint64_t new_size = mem_file->position + size;
  if (new_size > mem_file->capacity) {
    if (MemGrow(mem_file, new_size) != kMemZipOk) {
      GELOGE(FAILED, "[MEMZIP] Failed to expand memory capacity[%zu bytes]", new_size);
      mem_file->error = kMemZipError;
      return 0;
    }
  }
  const auto ret = memcpy_s(mem_file->buffer + mem_file->position, size, buf, size);
  if (ret != EOK) {
    GELOGE(FAILED,
           "[MEMZIP] Failed to copy, ret=%d: dest_base_ptr[%p], dest_position[%zu], dest_max[%zu], src_ptr[%p], "
           "src_size[%zu]",
           ret, mem_file->buffer, mem_file->position, mem_file->capacity - mem_file->position, buf, size);
    return 0;
  }

  mem_file->position += size;

  if (mem_file->position > mem_file->length) {
    mem_file->length = mem_file->position;
  }

  return size;
}

ZPOS64_T ZCALLBACK MemTell64FileFunc(voidpf opaque, voidpf stream) {
  const auto mem_file = static_cast<MemoryFile *>(stream);
  (void)opaque;

  if (mem_file == nullptr) {
    return static_cast<ZPOS64_T>(-1);
  }

  return mem_file->position;
}

template <typename T>
long MemSeek64FileImpl(T *mem_file, const ZPOS64_T offset, const int origin) {
  if (mem_file == nullptr) {
    return kMemZipError;
  }

  uint64_t new_position;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_CUR:
      new_position = mem_file->position + offset;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      new_position = mem_file->length + offset;
      break;
    case ZLIB_FILEFUNC_SEEK_SET:
      new_position = offset;
      break;
    default:
      return kMemZipError;
  }

  if (new_position > mem_file->length) {
    return kMemZipError;
  }

  mem_file->position = new_position;
  return kMemZipOk;
}

long ZCALLBACK MemSeek64FileFunc(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin) {
  const auto mem_file = static_cast<MemoryFile *>(stream);
  (void)opaque;

  if (mem_file == nullptr) {
    GELOGE(FAILED, "[MEMZIP] Get invalid memory file");
    return kMemZipError;
  }
  const auto ret = MemSeek64FileImpl(mem_file, offset, origin);
  if (ret != kMemZipOk) {
    GELOGE(FAILED, "[MEMZIP] Failed to seek, current_position=%zu, offset=%zu, origin=%d, file_length=%zu",
           mem_file->position, static_cast<uint64_t>(offset), origin, mem_file->length);
  }
  return ret;
}

int ZCALLBACK MemCloseFileFunc(voidpf opaque, voidpf stream) {
  const auto mem_file = static_cast<MemoryFile *>(stream);
  (void)opaque;

  if (mem_file == nullptr) {
    return kMemZipError;
  }

  if ((mem_file->buffer != nullptr) && (mem_file->release_from_outside == 0)) {
    std::free(mem_file->buffer);
    *mem_file = {};
  }

  return kMemZipOk;
}

int ZCALLBACK MemErrorFileFunc(voidpf opaque, voidpf stream) {
  const auto mem_file = static_cast<MemoryFile *>(stream);
  (void)opaque;

  if (mem_file == nullptr) {
    return kMemZipError;
  }

  return mem_file->error;
}

void FillMemFileFuncWithBuffer(zlib_filefunc64_def *file_func_def, MemoryFile *mem_file) {
  file_func_def->zopen64_file = MemOpenFileFuncWithBuffer;
  file_func_def->zread_file = MemReadFileFunc;
  file_func_def->zwrite_file = MemWriteFileFunc;
  file_func_def->ztell64_file = MemTell64FileFunc;
  file_func_def->zseek64_file = MemSeek64FileFunc;
  file_func_def->zclose_file = MemCloseFileFunc;
  file_func_def->zerror_file = MemErrorFileFunc;
  file_func_def->opaque = mem_file;
}

voidpf ZCALLBACK MemOpenFileFuncReadonly(voidpf opaque, const void *filename, int mode) {
  (void)filename;
  (void)mode;

  const auto mem_file_readonly = static_cast<SimpleZipMemoryFileReadonly *>(opaque);
  if (mem_file_readonly == nullptr) {
    GELOGE(FAILED, "[MEMZIP] Opaque pointer is null. Cannot initialize memory file.");
    return nullptr;
  }
  mem_file_readonly->position = 0;
  return mem_file_readonly;
}

uLong ZCALLBACK MemReadFileFuncReadonly(voidpf opaque, voidpf stream, void *buf, uLong size) {
  const auto mem_file = static_cast<SimpleZipMemoryFileReadonly *>(stream);
  (void)opaque;
  return MemReadFileImpl(mem_file, buf, size);
}

uLong ZCALLBACK MemWriteFileFuncReadonly(voidpf opaque, voidpf stream, const void *buf, uLong size) {
  (void)opaque;
  (void)stream;
  (void)buf;
  (void)size;
  return 0;
}

ZPOS64_T ZCALLBACK MemTell64FileFuncReadonly(voidpf opaque, voidpf stream) {
  const auto mem_file = static_cast<SimpleZipMemoryFileReadonly *>(stream);
  (void)opaque;

  if (mem_file == nullptr) {
    return static_cast<ZPOS64_T>(-1);
  }
  return mem_file->position;
}

long ZCALLBACK MemSeek64FileFuncReadonly(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin) {
  const auto mem_file = static_cast<SimpleZipMemoryFileReadonly *>(stream);
  (void)opaque;
  return MemSeek64FileImpl(mem_file, offset, origin);
}

int ZCALLBACK MemCloseFileFuncReadonly(voidpf opaque, voidpf stream) {
  (void)opaque;
  (void)stream;
  return kMemZipOk;
}

int ZCALLBACK MemErrorFileFuncReadonly(voidpf opaque, voidpf stream) {
  const auto mem_file = static_cast<SimpleZipMemoryFileReadonly *>(stream);
  (void)opaque;

  if (mem_file == nullptr) {
    return kMemZipError;
  }
  return kMemZipOk;
}

void FillMemFileFuncReadonly(zlib_filefunc64_def *file_func_def, SimpleZipMemoryFileReadonly *mem_file) {
  file_func_def->zopen64_file = MemOpenFileFuncReadonly;
  file_func_def->zread_file = MemReadFileFuncReadonly;
  file_func_def->zwrite_file = MemWriteFileFuncReadonly;
  file_func_def->ztell64_file = MemTell64FileFuncReadonly;
  file_func_def->zseek64_file = MemSeek64FileFuncReadonly;
  file_func_def->zclose_file = MemCloseFileFuncReadonly;
  file_func_def->zerror_file = MemErrorFileFuncReadonly;
  file_func_def->opaque = mem_file;
}

std::string GetBaseName(const std::string &path) {
  if (path.empty()) {
    return "";
  }

  const auto pos_slash = path.find_last_of('/');
  std::string file_name = (pos_slash == std::string::npos) ? path : path.substr(pos_slash + 1);
  if (file_name.empty()) {
    return "";
  }

  const auto pos_dot = file_name.find_last_of('.');
  if (pos_dot == std::string::npos || pos_dot == 0) return file_name;

  return file_name.substr(0, pos_dot);
}
}  // namespace

SimpleZipArchiveReader::SimpleZipArchiveReader(const uint8_t *data, size_t length) : mem_file_{data, length, 0} {
  if (mem_file_.buffer == nullptr || mem_file_.length == 0) {
    GELOGE(FAILED, "Invalid zip archive data, data is [%p] and size is [%zu]", mem_file_.buffer, mem_file_.length);
    return;
  }

  zlib_filefunc64_def file_funcs;
  FillMemFileFuncReadonly(&file_funcs, &mem_file_);
  zip_handle_ = unzOpen2_64(nullptr, &file_funcs);
  if (zip_handle_ == nullptr) {
    GELOGE(FAILED, "Failed to open ZIP file from memory");
  }
}

SimpleZipArchiveReader::~SimpleZipArchiveReader() {
  if (zip_handle_ != nullptr) {
    (void)unzClose(zip_handle_);
  }
}

std::vector<std::string> SimpleZipArchiveReader::ListFiles() const {
  GE_ASSERT_NOTNULL(zip_handle_, "Invalid status of archive");

  std::vector<std::string> file_list;
  auto uz_ret = unzGoToFirstFile(zip_handle_);
  GE_ASSERT_TRUE(uz_ret == UNZ_OK, "Failed to go to the first file in the archive, ret = %d", uz_ret);

  do {
    std::vector<char_t> name_buff(kMaxFileNameLength, '\0');
    uz_ret = unzGetCurrentFileInfo64(zip_handle_, nullptr, name_buff.data(), name_buff.size(), nullptr, 0, nullptr, 0);
    GE_ASSERT_TRUE(uz_ret == UNZ_OK, "Failed to get the current file information, ret = %d", uz_ret);
    const std::string file_name(name_buff.data());
    if (!file_name.empty() && file_name.back() != '/') {
      file_list.emplace_back(file_name);
    }
    uz_ret = unzGoToNextFile(zip_handle_);
  } while (uz_ret == UNZ_OK);

  GE_ASSERT_TRUE(uz_ret == UNZ_END_OF_LIST_OF_FILE, "unzGoToNextFile failed, ret=%d", uz_ret);
  return file_list;
}

ReadonlyByteBuffer SimpleZipArchiveReader::ExtractToMem(const std::string &entry_name, size_t &buffer_size) const {
  GE_ASSERT_NOTNULL(zip_handle_, "Invalid status of archive");

  auto uz_ret = unzLocateFile(zip_handle_, entry_name.c_str(), 0);
  GE_ASSERT_TRUE(uz_ret == UNZ_OK, "Failed to locate file [%s], ret = %d", entry_name.c_str(), uz_ret);

  uz_ret = unzOpenCurrentFile(zip_handle_);
  GE_ASSERT_TRUE(uz_ret == UNZ_OK, "Failed to open file [%s], ret = %d", entry_name.c_str(), uz_ret);
  GE_MAKE_GUARD(zipfile_guard, [this]() { (void)unzCloseCurrentFile(zip_handle_); });

  unz_file_info64 file_info{};
  uz_ret = unzGetCurrentFileInfo64(zip_handle_, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
  GE_ASSERT_TRUE(uz_ret == UNZ_OK, "Failed to get the current file information, ret = %d", uz_ret);
  GE_ASSERT_TRUE(file_info.uncompressed_size > 0);
  buffer_size = file_info.uncompressed_size;

  auto mutable_buffer = std::make_unique<uint8_t[]>(buffer_size);
  GE_ASSERT_NOTNULL(mutable_buffer, "Failed to allocate buffer, size = %zu", buffer_size);
  size_t total_read = 0;
  int32_t bytes_read = 0;
  do {
    const uint32_t remaining = static_cast<uint32_t>(
        std::min<size_t>(buffer_size - total_read, static_cast<size_t>(std::numeric_limits<int32_t>::max())));
    bytes_read = unzReadCurrentFile(zip_handle_, mutable_buffer.get() + total_read, remaining);

    GE_ASSERT_TRUE(bytes_read >= 0, "Failed to read file [%s], ret = %d", entry_name.c_str(), bytes_read);
    total_read += static_cast<size_t>(bytes_read);
  } while (bytes_read > 0);

  GE_ASSERT_TRUE(total_read == buffer_size, "Failed to extract file [%s], expected = %zu bytes, actual = %zu bytes",
                 entry_name.c_str(), buffer_size, total_read);
  GELOGI("Successfully extract file [%s], total_read = %zu bytes", entry_name.c_str(), total_read);
  return ReadonlyByteBuffer(mutable_buffer.release(), ConditionalDeleter{true});
}

ZipArchiveWriter::ZipArchiveWriter(const std::string &archive_path)
    : archive_path_(archive_path), base_name_(GetBaseName(archive_path)) {
  if (!InitArchive()) {
    GELOGE(FAILED, "Failed to initialize archive [%s]", archive_path.c_str());
  }
}

ZipArchiveWriter::~ZipArchiveWriter() {
  (void)WriteEndOfFile();
  if (mem_file_.buffer != nullptr) {
    std::free(mem_file_.buffer);
    mem_file_ = {};
  }
}

bool ZipArchiveWriter::WriteFile(const std::string &entry_name, const std::string &src_file_path, const bool compress) {
  GE_ASSERT_TRUE(IsMemFileOpened(), "Invalid status of archive [%s]", archive_path_.c_str());
  GE_ASSERT_TRUE(!entry_name.empty(), "Entry name cannot be empty");
  const std::string arc_name_with_prefix = base_name_ + "/" + entry_name;
  if (files_written_.count(arc_name_with_prefix) != 0) {
    GELOGW("Duplicate file entry '%s' detected and ignored, src_file_name [%s]", arc_name_with_prefix.c_str(),
           src_file_path.c_str());
    return true;
  }

  std::ifstream fin(src_file_path, std::ios::binary);
  GE_ASSERT_TRUE(fin.is_open(), "Failed to open file [%s]", src_file_path.c_str());
  fin.seekg(0, std::ios::end);
  const auto file_size = fin.tellg();
  fin.seekg(0, std::ios::beg);

  zip_fileinfo file_info{};
  const auto compression_method = compress ? Z_DEFLATED : Z_BINARY;
  const auto compression_level = compress ? Z_DEFAULT_COMPRESSION : Z_NO_COMPRESSION;
  auto ret = zipOpenNewFileInZip64(zip_handle_, arc_name_with_prefix.c_str(), &file_info, nullptr, 0, nullptr, 0,
                                   nullptr, compression_method, compression_level, 1);
  GE_ASSERT_TRUE(ret == ZIP_OK, "Failed to open zip entry [%s], ret = %d", arc_name_with_prefix.c_str(), ret);
  GE_MAKE_GUARD(close_file_in_zip, [this]() { (void)zipCloseFileInZip(zip_handle_); });

  std::vector<char_t> buffer(kBufSize);
  auto remaining = static_cast<int64_t>(file_size);
  while (remaining > 0) {
    const int64_t chunk = remaining > kBufSize ? kBufSize : remaining;
    fin.read(buffer.data(), chunk);
    auto read_bytes = fin.gcount();
    GE_ASSERT_TRUE(read_bytes == chunk, "Failed to read from file [%s], expected = %zu bytes, bytes_read = %zu bytes",
                   src_file_path.c_str(), chunk, read_bytes);

    ret = zipWriteInFileInZip(zip_handle_, buffer.data(), static_cast<unsigned>(chunk));
    GE_ASSERT_TRUE(ret == ZIP_OK, "zipWriteInFileInZip failed for [%s], ret = %d, bytes_left = %zu bytes",
                   arc_name_with_prefix.c_str(), ret, remaining);
    remaining -= chunk;
  }
  GELOGI("Successfully written [%s] to archive, file_size = %zu bytes", src_file_path.c_str(), file_size);
  files_written_.insert(arc_name_with_prefix);
  return true;
}

bool ZipArchiveWriter::WriteBytes(const std::string &entry_name, const void *data, const size_t data_size,
                                  const bool compress) {
  GE_ASSERT_TRUE(IsMemFileOpened(), "Invalid status of archive [%s]", archive_path_.c_str());
  GE_ASSERT_TRUE(!entry_name.empty(), "Entry name cannot be empty");
  GE_ASSERT_NOTNULL(data, "Pointer data is null, arc_name is [%s]", entry_name.c_str());
  GE_ASSERT_TRUE(data_size > 0, "Data size must be greater than zero, arc_name is [%s]", entry_name.c_str());
  const std::string arc_name_with_prefix = base_name_ + "/" + entry_name;
  if (files_written_.count(arc_name_with_prefix) != 0) {
    GELOGW("Duplicate file entry '%s' detected and ignored", arc_name_with_prefix.c_str());
    return true;
  }

  zip_fileinfo file_info{};
  const auto compression_method = compress ? Z_DEFLATED : Z_BINARY;
  const auto compression_level = compress ? Z_DEFAULT_COMPRESSION : Z_NO_COMPRESSION;
  auto ret = zipOpenNewFileInZip64(zip_handle_, arc_name_with_prefix.c_str(), &file_info, nullptr, 0, nullptr, 0,
                                   nullptr, compression_method, compression_level, 1);
  GE_ASSERT_TRUE(ret == ZIP_OK, "Failed to open file [%s] in zip, ret = %d", arc_name_with_prefix.c_str(), ret);
  GE_MAKE_GUARD(close_file_in_zip, [this]() { (void)zipCloseFileInZip(zip_handle_); });

  auto pdata = static_cast<const uint8_t *>(data);
  size_t remaining = data_size;
  while (remaining > 0) {
    const auto chunk = remaining > kMaxWriteSize ? kMaxWriteSize : static_cast<uint32_t>(remaining);

    ret = zipWriteInFileInZip(zip_handle_, pdata, chunk);
    GE_ASSERT_TRUE(ret == ZIP_OK, "zipWriteInFileInZip failed for [%s], ret = %d, bytes_left = %zu bytes",
                   arc_name_with_prefix.c_str(), ret, remaining);

    pdata += chunk;
    remaining -= chunk;
  }

  GELOGI("Successfully written [%s] to archive, data_size = %zu bytes", entry_name.c_str(), data_size);
  files_written_.insert(arc_name_with_prefix);
  return true;
}

bool ZipArchiveWriter::WriteEndOfFile() {
  if (IsMemFileOpened()) {
    GE_MAKE_GUARD(zip_close_guard, [this]() { zip_handle_ = nullptr; });
    return zipClose(zip_handle_, nullptr) == ZIP_OK;
  }
  return true;
}

bool ZipArchiveWriter::SaveModelData(ModelBufferData &model, bool save_to_file) {
  return save_to_file ? SaveModelDataToFile() : SaveModelDataToBuffer(model);
}

bool ZipArchiveWriter::SaveModelDataToFile() {
  GE_ASSERT_TRUE(WriteEndOfFile());
  GE_ASSERT_SUCCESS(SaveBinToFile(reinterpret_cast<const char *>(mem_file_.buffer), mem_file_.length, archive_path_));
  return true;
}

bool ZipArchiveWriter::SaveModelDataToBuffer(ModelBufferData &model) {
  GE_ASSERT_TRUE(WriteEndOfFile());
  GE_ASSERT_NOTNULL(mem_file_.buffer);
  GE_ASSERT_TRUE(mem_file_.length > 0U);
  model.length = mem_file_.length;
  model.data = std::shared_ptr<uint8_t>(mem_file_.buffer, [](uint8_t *ptr) { std::free(ptr); });
  mem_file_ = {};
  return true;
}

bool ZipArchiveWriter::InitArchive() {
  GE_ASSERT_TRUE(!base_name_.empty(), "The base_name_ of is empty");
  zlib_filefunc64_def file_funcs;
  FillMemFileFuncWithBuffer(&file_funcs, &mem_file_);
  zip_handle_ = zipOpen2_64(archive_path_.c_str(), APPEND_STATUS_CREATE, nullptr, &file_funcs);
  GE_ASSERT_NOTNULL(zip_handle_, "Failed to open archive [%s]", archive_path_.c_str());
  return true;
}

}  // namespace ge
