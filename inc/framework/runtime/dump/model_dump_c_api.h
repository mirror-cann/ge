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
struct Om2Tensor {
    uint64_t device_address;     // 保留字段，暂时填 0
    uint64_t size;
    int32_t data_type;
    int32_t format;
    const int64_t* shape_dims;
    uint64_t shape_dims_num;
};

struct Om2TaskIoEntry {
    const struct Om2Tensor* tensor;
    uint64_t offset;             // 这个 tensor 的地址在 args buffer 中的偏移（字节）
};

// ============ Task Dump 信息 ============
struct Om2TaskInfo {
    // 基础信息
    const char* op_name;
    const char* op_type;
    uint32_t task_id;
    uint32_t stream_id;
    uint32_t context_id;
    uint32_t thread_id;
    uint64_t op_desc_id;
    uintptr_t args_base;
    uint64_t args_size;

    // 输入输出
    uint64_t input_num;
    const struct Om2TaskIoEntry* inputs;
    uint32_t output_num;
    const struct Om2TaskIoEntry* outputs;

    // Workspace
    uint32_t workspace_num;
    const uint64_t* workspace_addrs;
    const uint64_t* workspace_sizes;

    // 其他
    uint32_t task_type;
    void* stream;                // rtStream_t
    uint32_t is_raw_address;
};

// ============ 弱符号接口 ============
/**
 * @brief 添加 Task Dump 信息
 * @param model_id 模型 ID（预留，暂时不用）
 * @param instance_handle ModelDumpManager 实例指针
 * @param task_info Task Dump 信息
 * @param extended_attrs 扩展属性（预留，暂时不用）
 * @param extended_attrs_size 扩展属性大小（预留，暂时不用）
 * @return Status 状态码
 */
int32_t OM2_C_API_EXPORT ReportTaskInfo(uint32_t model_id,
                                      void* instance_handle,
                                      const struct Om2TaskInfo* task_info,
                                      const void* extended_attrs,
                                      size_t extended_attrs_size);

/**
 * @brief 查询指定算子是否需要 Data Dump
 * @param model_id 模型 ID（预留，暂时不用）
 * @param instance_handle ModelDumpManager 实例指针
 * @param op_name 算子名称
 * @param is_data_dump [出参]该算子是否需要 data dump，0=false, 1=true
 * @return Status 状态码
 */
int32_t OM2_C_API_EXPORT IsDataDumpEnabled(uint32_t model_id,
                                            void* instance_handle,
                                            const char* op_name,
                                            uint8_t* is_data_dump);

#ifdef __cplusplus
}
#endif

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_C_API_H_
