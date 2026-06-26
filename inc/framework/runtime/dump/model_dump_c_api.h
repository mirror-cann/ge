/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_C_API_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_C_API_H_

#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER)
#define OM2_C_API_EXPORT __declspec(dllexport)
#else
#define OM2_C_API_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============ Tensor信息 ============
/**
 * @brief OM2 算子输入或输出 Tensor 的基础信息。
 */
struct Om2Tensor {
  uint64_t device_address;    // 输入，预留字段，暂时填 0。
  uint64_t size;              // 输入，Tensor 数据大小，单位为字节。
  int32_t data_type;          // 输入，Tensor 数据类型，取值与 ge::DataType 保持一致。
  int32_t format;             // 输入，Tensor 数据格式，取值与 ge::Format 保持一致。
  const int64_t *shape_dims;  // 输入，Tensor shape 维度数组首地址。shape_dims_num 为 0 时可以为空指针。
  uint64_t shape_dims_num;    // 输入，Tensor shape 维度个数。
};

/**
 * @brief OM2 算子单个输入或输出 Tensor 与 args buffer 地址槽位的映射信息。
 */
struct Om2TaskIoEntry {
  const struct Om2Tensor *tensor;  // 输入，Tensor 基础信息指针，不允许为空。
  uint64_t offset;                 // 输入，Tensor 地址在 args buffer 中的偏移，单位为字节。
};

/**
 * @brief L0 异常 dump 的 kernel 参数槽位类型。
 */
enum Om2L0ArgKind {
  OM2_L0_ARG_INPUT = 0,           // 输入 Tensor 地址槽位。
  OM2_L0_ARG_OUTPUT = 1,          // 输出 Tensor 地址槽位。
  OM2_L0_ARG_WORKSPACE = 2,       // Workspace 地址槽位。
  OM2_L0_ARG_TILING = 3,          // Tiling 数据地址槽位。
  OM2_L0_ARG_SHAPE_INFO = 4,      // Shape 信息地址槽位。
  OM2_L0_ARG_LEVEL1_DESC = 5,     // 一级描述信息地址槽位。
  OM2_L0_ARG_PLACEHOLDER = 6,     // 占位地址槽位。
  OM2_L0_ARG_CUSTOM_VALUE = 7,    // 自定义立即数槽位。
  OM2_L0_ARG_FFTS_ADDR = 8,       // FFTS 地址槽位。
  OM2_L0_ARG_EVENT_ADDR = 9,      // Event 地址槽位。
  OM2_L0_ARG_OVERFLOW_ADDR = 10,  // Overflow 地址槽位。
  OM2_L0_ARG_EMPTY_ADDR = 11      // 空地址槽位。
};

/**
 * @brief L0 异常 dump 的单个 kernel 参数槽位原始信息。
 */
struct Om2L0ArgSlotInfo {
  uint32_t kind;                  // 输入，槽位类型，取值见 Om2L0ArgKind。
  uint32_t flags;                 // 输入，预留标志位，当前填 0。
  uint64_t args_offset;           // 输入，槽位在 args buffer 中的偏移，单位为字节。
  uint64_t value;                 // 输入，槽位附加值。Tiling 或 Shape 信息场景表示数据大小，其他场景按 kind 解释。
  uint32_t related_index;         // 输入，关联索引。输入、输出或 Workspace 场景表示对应数组下标。
  uint32_t event_id;              // 输入，Event 场景的事件 ID，非 Event 场景填 0。
  uint64_t level1_target_offset;  // 输入，一级描述信息指向的目标槽位偏移，单位为字节。
};

/**
 * @brief L0 异常 dump 的 kernel 参数原始信息列表。
 */
struct Om2L0TaskRawInfo {
  uint32_t version;  // 输入，结构体版本号，当前为 1。
  // 输入，是否需要 assert 或 printf 相关异常 dump，0 表示不需要，非 0 表示需要。
  uint32_t need_assert_or_printf;
  uint64_t arg_num;  // 输入，args 指向的槽位个数。
  // 输入，槽位信息数组首地址。arg_num 为 0 时可以为空指针。
  const struct Om2L0ArgSlotInfo *args;
};

// ============ Task Dump 信息 ============
/**
 * @brief OM2 算子任务 dump 信息。
 */
struct Om2TaskInfo {
  // 基础信息
  const char *op_name;  // 输入，算子名称，不允许为空。
  const char *op_type;  // 输入，算子类型，不允许为空。
  uint32_t task_id;     // 输入，运行时 task ID。
  uint32_t stream_id;   // 输入，运行时 stream ID。
  uint32_t context_id;  // 输入，运行时 context ID。
  uint32_t thread_id;   // 输入，运行时 thread ID。
  uint32_t block_dim;   // 输入，Kernel launch block dim，非 Kernel 任务填 0。
  uint64_t op_desc_id;  // 输入，OpDesc ID。
  uintptr_t args_base;  // 输入，args buffer 基地址。
  uint64_t args_size;   // 输入，args buffer 大小，单位为字节。

  // 输入输出
  uint64_t input_num;  // 输入，inputs 指向的输入 Tensor 个数。
  // 输入，输入 Tensor 映射数组首地址。input_num 为 0 时可以为空指针。
  const struct Om2TaskIoEntry *inputs;
  uint32_t output_num;  // 输入，outputs 指向的输出 Tensor 个数。
  // 输入，输出 Tensor 映射数组首地址。output_num 为 0 时可以为空指针。
  const struct Om2TaskIoEntry *outputs;

  // Workspace
  uint32_t workspace_num;           // 输入，Workspace 个数。
  const uint64_t *workspace_addrs;  // 输入，Workspace 地址数组首地址。workspace_num 为 0 时可以为空指针。
  const uint64_t *workspace_sizes;  // 输入，Workspace 大小数组首地址，单位为字节。workspace_num 为 0 时可以为空指针。

  // 其他
  uint32_t task_type;       // 输入，任务类型，取值与 ModelTaskType 保持一致。
  void *stream;             // 输入，rtStream_t 运行时流句柄。
  uint32_t is_raw_address;  // 输入，是否为 raw address 模式，0 表示否，非 0 表示是。
  // 输入，L0 异常 dump 原始信息指针。不需要 L0 异常 dump 时可以为空指针。
  const struct Om2L0TaskRawInfo *l0_exception_dump_info;
};

// ============ 弱符号接口 ============
/**
 * @brief 在 OM2 算子任务 launch 前执行 DFX 预处理。
 * @param model_id 输入，模型 ID。当前预留，接口内部暂不使用。
 * @param instance_handle 输入，ModelDumpManager 实例指针，不允许为空。
 * @param task_info 输入，算子任务 dump 信息指针，不允许为空。
 * @param extended_attrs 输入，预留扩展属性指针，当前必须为空指针。
 * @param extended_attrs_size 输入，预留扩展属性大小，单位为字节，当前必须为 0。
 * @return 返回 0 表示成功，返回其他值表示失败。
 */
int32_t OM2_C_API_EXPORT ReportDfxTaskPreprocess(uint32_t model_id, void *instance_handle,
                                                 const struct Om2TaskInfo *task_info, const void *extended_attrs,
                                                 size_t extended_attrs_size);

/**
 * @brief 在 OM2 算子任务 launch 后保存 DFX 任务信息。
 * @param model_id 输入，模型 ID。当前预留，接口内部暂不使用。
 * @param instance_handle 输入，ModelDumpManager 实例指针，不允许为空。
 * @param task_info 输入，算子任务 dump 信息指针，不允许为空。
 * @param extended_attrs 输入，预留扩展属性指针，当前必须为空指针。
 * @param extended_attrs_size 输入，预留扩展属性大小，单位为字节，当前必须为 0。
 * @return 返回 0 表示成功，返回其他值表示失败。
 */
int32_t OM2_C_API_EXPORT ReportDfxTaskPostprocess(uint32_t model_id, void *instance_handle,
                                                  const struct Om2TaskInfo *task_info, const void *extended_attrs,
                                                  size_t extended_attrs_size);

/**
 * @brief 查询指定算子是否需要 Data Dump。
 * @param model_id 输入，模型 ID。当前预留，接口内部暂不使用。
 * @param instance_handle 输入，ModelDumpManager 实例指针，不允许为空。
 * @param op_name 输入，算子名称。
 * @param is_data_dump 输出，该算子是否需要 Data Dump，0 表示不需要，1 表示需要。
 * @return 返回 0 表示成功，返回其他值表示失败。
 */
int32_t OM2_C_API_EXPORT IsDataDumpEnabled(uint32_t model_id, void *instance_handle, const char *op_name,
                                           uint8_t *is_data_dump);

#ifdef __cplusplus
}
#endif

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_C_API_H_
