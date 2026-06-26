#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------
import acl
from common import (
    GRAPH_ID,
    build_overload_graph,
    check_ret,
    create_input_tensors,
    dump_overload_graph,
    run_graph,
)
from ge.graph import Graph
from ge.session import Session


def run_with_default_allocator(ge_graph: Graph, session: Session) -> int:
    stream = None
    graph_added = False
    try:
        # 1. 创建 Stream
        stream, ret = acl.rt.create_stream()
        check_ret("acl.rt.create_stream", ret)

        # 2. 创建 Device 输入
        inputs = create_input_tensors()

        # 3. 添加并异步执行 Graph
        session.add_graph(GRAPH_ID, ge_graph)
        graph_added = True
        device_outputs = session.run_graph_with_stream_async(GRAPH_ID, stream, list(inputs))

        # 4. 等待 Stream 上的任务完成
        check_ret("acl.rt.synchronize_stream", acl.rt.synchronize_stream(stream))
        print("[Info] 异步执行 Graph 成功！")

        # 5. 将 Device 数据传回 Host 并打印
        host_outputs = [out.to_host() for out in device_outputs]
        for idx, tensor in enumerate(host_outputs, start=1):
            print(f"Tensor{idx} 详情：{tensor}")
        return 0
    finally:
        if stream is not None:
            check_ret("acl.rt.destroy_stream", acl.rt.destroy_stream(stream))
        if graph_added and session is not None:
            session.remove_graph(GRAPH_ID)


graph = build_overload_graph()
dump_overload_graph(graph)
run_graph(graph, run_with_default_allocator)
