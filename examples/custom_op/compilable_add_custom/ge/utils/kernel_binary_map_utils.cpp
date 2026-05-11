/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "graph/custom_op.h"

using namespace ge;
using KernelBinary = std::vector<uint8_t>;
using KernelBinaryMap = std::map<std::string, KernelBinary>;

namespace kernel_binary_map_utils {
namespace {
// buffer 采用固定小端布局：
// [magic][version][kernel_count][key_len][key][value_len][value]...
constexpr uint32_t kKernelBinaryMapMagic = 0x56415231U;
constexpr uint32_t kKernelBinaryMapVersion = 1U;
constexpr size_t kUint32ByteCount = sizeof(uint32_t);
constexpr uint32_t kBitsPerByte = 8U;

// 将 uint32_t 逐字节写入 buffer，保证跨进程/跨阶段读取时格式稳定。
void AppendUint32(std::vector<uint8_t> &buffer, uint32_t value) {
  for (size_t i = 0U; i < kUint32ByteCount; ++i) {
    const auto shift = static_cast<uint32_t>(i) * kBitsPerByte;
    buffer.push_back(static_cast<uint8_t>((value >> shift) & 0xFFU));
  }
}

// 按与 AppendUint32 相同的小端格式回读 uint32_t，并推进 offset。
bool ReadUint32(const std::vector<uint8_t> &buffer, size_t &offset, uint32_t &value) {
  if ((offset > buffer.size()) || (sizeof(value) > (buffer.size() - offset))) {
    return false;
  }
  value = 0U;
  for (size_t i = 0U; i < kUint32ByteCount; ++i) {
    const auto shift = static_cast<uint32_t>(i) * kBitsPerByte;
    value |= static_cast<uint32_t>(buffer[offset + i]) << shift;
  }
  offset += sizeof(value);
  return true;
}

bool HasEnoughBytes(const std::vector<uint8_t> &buffer, size_t offset, size_t size) {
  return (offset <= buffer.size()) && (size <= (buffer.size() - offset));
}

// 当前协议使用 uint32_t 记录长度，序列化前先限制 key/value 数量和大小。
bool CheckSizeFitsUint32(size_t size, const char *tag) {
  if (size <= std::numeric_limits<uint32_t>::max()) {
    return true;
  }
  std::cerr << __FILE__ << ":" << __LINE__ << " " << tag << " size exceeds uint32 range, size: " << size << std::endl;
  return false;
}
}  // namespace

graphStatus Serialize(const KernelBinaryMap &kernel_binary_map, std::vector<uint8_t> &buffer) {
  // 先写文件头，再逐项写入：
  // key 的字节长度 + key 内容 + 二进制长度 + 二进制内容。
  if (!CheckSizeFitsUint32(kernel_binary_map.size(), "kernel binary map")) {
    return GRAPH_FAILED;
  }

  std::vector<uint8_t> serialized;
  AppendUint32(serialized, kKernelBinaryMapMagic);
  AppendUint32(serialized, kKernelBinaryMapVersion);
  AppendUint32(serialized, static_cast<uint32_t>(kernel_binary_map.size()));

  for (const auto &item : kernel_binary_map) {
    if (!CheckSizeFitsUint32(item.first.size(), "kernel binary key")) {
      return GRAPH_FAILED;
    }
    if (!CheckSizeFitsUint32(item.second.size(), "kernel binary value")) {
      return GRAPH_FAILED;
    }
    AppendUint32(serialized, static_cast<uint32_t>(item.first.size()));
    serialized.insert(serialized.end(), item.first.begin(), item.first.end());
    AppendUint32(serialized, static_cast<uint32_t>(item.second.size()));
    serialized.insert(serialized.end(), item.second.begin(), item.second.end());
  }

  buffer.swap(serialized);
  return GRAPH_SUCCESS;
}

graphStatus Deserialize(const std::vector<uint8_t> &buffer, KernelBinaryMap &kernel_binary_map) {
  // 严格按 Serialize 的协议反序列化；任何截断、重复 key、尾部脏数据都直接失败。
  size_t offset = 0U;
  uint32_t magic = 0U;
  uint32_t version = 0U;
  uint32_t kernel_count = 0U;
  if (!ReadUint32(buffer, offset, magic) || !ReadUint32(buffer, offset, version) ||
      !ReadUint32(buffer, offset, kernel_count)) {
    std::cerr << __FILE__ << ":" << __LINE__ << " failed to parse kernel binary map header" << std::endl;
    return GRAPH_FAILED;
  }
  if (magic != kKernelBinaryMapMagic) {
    std::cerr << __FILE__ << ":" << __LINE__ << " unexpected kernel binary map magic: " << magic << std::endl;
    return GRAPH_FAILED;
  }
  if (version != kKernelBinaryMapVersion) {
    std::cerr << __FILE__ << ":" << __LINE__ << " unsupported kernel binary map version: " << version << std::endl;
    return GRAPH_FAILED;
  }

  KernelBinaryMap deserialized;
  for (uint32_t i = 0U; i < kernel_count; ++i) {
    uint32_t key_size = 0U;
    if (!ReadUint32(buffer, offset, key_size) || !HasEnoughBytes(buffer, offset, key_size)) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to parse kernel binary key, index: " << i << std::endl;
      return GRAPH_FAILED;
    }
    std::string key(reinterpret_cast<const char *>(buffer.data() + offset), key_size);
    offset += key_size;

    uint32_t value_size = 0U;
    if (!ReadUint32(buffer, offset, value_size) || !HasEnoughBytes(buffer, offset, value_size)) {
      std::cerr << __FILE__ << ":" << __LINE__ << " failed to parse kernel binary value, index: " << i << std::endl;
      return GRAPH_FAILED;
    }
    KernelBinary value(buffer.begin() + static_cast<std::ptrdiff_t>(offset),
                       buffer.begin() + static_cast<std::ptrdiff_t>(offset + value_size));
    offset += value_size;

    if (!deserialized.emplace(std::move(key), std::move(value)).second) {
      std::cerr << __FILE__ << ":" << __LINE__ << " duplicated kernel binary key found during deserialize" << std::endl;
      return GRAPH_FAILED;
    }
  }

  if (offset != buffer.size()) {
    std::cerr << __FILE__ << ":" << __LINE__
              << " kernel binary map buffer has trailing bytes, remaining: " << (buffer.size() - offset) << std::endl;
    return GRAPH_FAILED;
  }

  kernel_binary_map.swap(deserialized);
  return GRAPH_SUCCESS;
}
}  // namespace kernel_binary_map_utils
