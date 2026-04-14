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
"""样例公共逻辑：常量、图构建、输入张量创建、GE 生命周期管理。"""
import traceback
from typing import Callable, Tuple

import acl

from ge.es.graph_builder import GraphBuilder
from ge.ge_global import GeApi
from ge.graph import DumpFormat, Graph, Tensor
from ge.graph.types import DataType, Format, Placement
from ge.session import Session

ACL_SUCCESS = 0
DEVICE_ID = 0
GRAPH_ID = 1
ACL_MEM_MALLOC_NORMAL_ONLY = 2


def check_ret(name: str, ret: int) -> None:
    """断言 ACL 调用成功，失败时抛出 RuntimeError。"""
    if ret != ACL_SUCCESS:
        raise RuntimeError(f"{name} failed, ret={ret}")


def build_overload_graph() -> Graph:
    """使用操作符重载构建静态 shape 加法图（shape 固定为 [2, 3]）。"""
    builder = GraphBuilder("MakeAddGraph")
    h1 = builder.create_input(index=0, name="input1", data_type=DataType.DT_FLOAT, shape=[2, 3])
    h2 = builder.create_input(index=1, name="input2", data_type=DataType.DT_INT64, shape=[2, 3])
    h3 = builder.create_input(index=2, name="input3", data_type=DataType.DT_INT64, shape=[2, 3])
    builder.set_graph_output(h1 + h2 + h3, 0)
    return builder.build_and_reset()


def dump_overload_graph(graph: Graph) -> None:
    graph.dump_to_file(format=DumpFormat.kOnnx, suffix="make_add_graph")


def create_input_tensors() -> Tuple[Tensor, Tensor, Tensor]:
    input0 = Tensor(
        [1.0, 1.0, 2.0, 2.0, 3.0, 3.0], None,
        DataType.DT_FLOAT, Format.FORMAT_ND, [2, 3], Placement.PLACEMENT_DEVICE,
    )
    input1 = Tensor(
        [1, 1, 2, 2, 3, 3], None,
        DataType.DT_INT64, Format.FORMAT_ND, [2, 3], Placement.PLACEMENT_DEVICE,
    )
    input2 = Tensor(
        [1, 1, 2, 2, 3, 3], None,
        DataType.DT_INT64, Format.FORMAT_ND, [2, 3], Placement.PLACEMENT_DEVICE,
    )
    return input0, input1, input2


def run_graph(graph: Graph, session_runner: Callable[[Graph, Session], int]) -> int:
    config = {
        "ge.exec.deviceId": str(DEVICE_ID),
        "ge.graphRunMode": "0",
    }
    ge_api = GeApi()
    ge_api.ge_initialize(config)
    print(f"[Info] GE 环境初始化成功 (Device ID: {DEVICE_ID})")

    try:
        check_ret("acl.init", acl.init())
        session = Session()
        check_ret("acl.rt.set_device", acl.rt.set_device(DEVICE_ID))
        return session_runner(graph, session)
    except Exception as e:
        print(f"[Error] 执行过程中出错: {e}")
        traceback.print_exc()
        return -1
    finally:
        acl.rt.reset_device(DEVICE_ID)
        acl.finalize()
        ge_api.ge_finalize()
        print("[Info] 运行环境已清理")
