#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------
import numpy as np

ACL_SUCCESS = 0
ACL_MEM_MALLOC_NORMAL_ONLY = 2
ACL_MEMCPY_HOST_TO_DEVICE = 1
ACL_MEMCPY_DEVICE_TO_HOST = 2
DEVICE_ID = 0


def check_ret(name: str, ret: int) -> None:
    if ret != ACL_SUCCESS:
        raise RuntimeError("{} failed ret={}".format(name, ret))


def prepare_acl_mdl_dataset(model_desc, io_type: str):
    """为 aclmdlDesc 创建输入或输出 dataset（Device 侧 buffer 列表）。"""
    import acl

    if io_type == "input":
        # 根据aclmdlDesc类型的数据，获取模型的输入个数
        io_num = acl.mdl.get_num_inputs(model_desc)
        # 根据aclmdlDesc类型的数据，获取指定输入的大小，单位为Byte
        get_size_by_index = acl.mdl.get_input_size_by_index
    else:
        # 根据aclmdlDesc类型的数据，获取模型的输出个数
        io_num = acl.mdl.get_num_outputs(model_desc)
        # 根据aclmdlDesc类型的数据，获取指定输出的大小，单位为Byte
        get_size_by_index = acl.mdl.get_output_size_by_index

    # 创建aclmdlDataset类型的数据
    dataset = acl.mdl.create_dataset()
    dataset_items = []
    # 循环为每个输入/输出申请内存，并将每个输入/输出添加到aclmdlDataset类型的数据中
    for index in range(io_num):
        buffer_size = get_size_by_index(model_desc, index)
        # 申请内存
        buffer, ret = acl.rt.malloc(buffer_size, ACL_MEM_MALLOC_NORMAL_ONLY)
        check_ret(f"acl.rt.malloc({io_type})", ret)
        data_buffer = acl.create_data_buffer(buffer, buffer_size)
        _, ret = acl.mdl.add_dataset_buffer(dataset, data_buffer)
        check_ret(f"acl.mdl.add_dataset_buffer({io_type})", ret)
        dataset_items.append({"buffer": buffer, "size": buffer_size})
    return dataset, dataset_items


def copy_inputs_to_acl_dataset(input_data, inputs):
    """将 Host 侧输入数据拷贝到 aclmdlDataset 对应的 Device buffer。"""
    import acl

    for index, input_array in enumerate(inputs):
        bytes_data = input_array.tobytes()
        bytes_ptr = acl.util.bytes_to_ptr(bytes_data)
        check_ret(
            f"acl.rt.memcpy(input_{index})",
            acl.rt.memcpy(
                input_data[index]["buffer"],
                input_data[index]["size"],
                bytes_ptr,
                len(bytes_data),
                ACL_MEMCPY_HOST_TO_DEVICE,
            ),
        )


def collect_acl_model_outputs(model_desc, output_data):
    """将模型输出从 Device 拷回 Host，并按输出 shape 转成 numpy 数组。"""
    import acl

    outputs = []
    for index, item in enumerate(output_data):
        host_buffer, ret = acl.rt.malloc_host(item["size"])
        check_ret(f"acl.rt.malloc_host(output_{index})", ret)
        try:
            check_ret(
                f"acl.rt.memcpy(output_{index})",
                acl.rt.memcpy(
                    host_buffer,
                    item["size"],
                    item["buffer"],
                    item["size"],
                    ACL_MEMCPY_DEVICE_TO_HOST,
                ),
            )
            output_bytes = acl.util.ptr_to_bytes(host_buffer, item["size"])
        finally:
            check_ret(f"acl.rt.free_host(output_{index})", acl.rt.free_host(host_buffer))

        dims, ret = acl.mdl.get_output_dims(model_desc, index)
        check_ret(f"acl.mdl.get_output_dims(output_{index})", ret)
        shape = tuple(dims["dims"][: dims["dimCount"]])
        outputs.append(np.frombuffer(output_bytes, dtype=np.float32).reshape(shape))
    return outputs


def release_acl_mdl_dataset(dataset) -> None:
    """释放 aclmdlDataset 及其关联的 Device buffer。"""
    import acl

    if not dataset:
        return
    num = acl.mdl.get_dataset_num_buffers(dataset)
    for index in range(num):
        data_buf = acl.mdl.get_dataset_buffer(dataset, index)
        if data_buf:
            data = acl.get_data_buffer_addr(data_buf)
            check_ret("acl.rt.free", acl.rt.free(data))
            check_ret("acl.destroy_data_buffer", acl.destroy_data_buffer(data_buf))
    check_ret("acl.mdl.destroy_dataset", acl.mdl.destroy_dataset(dataset))


def build_add_graph():
    from ge.es.graph_builder import GraphBuilder
    from ge.graph.types import DataType

    builder = GraphBuilder("OfflineAddGraph")
    input_x = builder.create_input(index=0, name="input_x", data_type=DataType.DT_FLOAT, shape=[2, 3])
    input_y = builder.create_input(index=1, name="input_y", data_type=DataType.DT_FLOAT, shape=[2, 3])
    builder.set_graph_output(input_x + input_y, 0)
    return builder.build_and_reset()


def build_mul_graph():
    from ge.es.graph_builder import GraphBuilder
    from ge.graph.types import DataType

    builder = GraphBuilder("OfflineMulGraph")
    input_x = builder.create_input(index=0, name="input_x", data_type=DataType.DT_FLOAT, shape=[2, 3])
    input_y = builder.create_input(index=1, name="input_y", data_type=DataType.DT_FLOAT, shape=[2, 3])
    builder.set_graph_output(input_x * input_y, 0)
    return builder.build_and_reset()


def create_sample_inputs():
    input_x = np.array([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]], dtype=np.float32)
    input_y = np.array([[10.0, 20.0, 30.0], [40.0, 50.0, 60.0]], dtype=np.float32)
    return [input_x, input_y]


def create_mul_expected_output():
    input_x, input_y = create_sample_inputs()
    return input_x * input_y
