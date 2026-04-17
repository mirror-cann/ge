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
from typing import Dict, List

import acl

from ge.allocator import Allocator, MemBlock
from ge.graph import Graph
from ge.session import Session

from common import (
    ACL_SUCCESS, GRAPH_ID, ACL_MEM_MALLOC_NORMAL_ONLY,
    check_ret, build_overload_graph, dump_overload_graph,
    create_input_tensors, run_graph,
)


class SamplePoolAllocator(Allocator):
    """内存池：按精确 size 缓存空闲块，命中则复用，否则从设备申请。

    适合请求尺寸固定（如推理输出 Tensor 大小不变）的场景，无桶对齐开销。
    缓存块在对象被 GC 时通过 __del__ 自动释放。
    """

    def __init__(self) -> None:
        # { size: [addr, ...] }  —— 按精确大小分组的空闲地址列表
        self._cache: Dict[int, List[int]] = {}

    def __del__(self) -> None:
        count = sum(len(v) for v in self._cache.values())
        for addrs in self._cache.values():
            for addr in addrs:
                ret = acl.rt.free(addr)
                if ret != ACL_SUCCESS:
                    print(f"[SamplePool] free address failed, ret={ret}")
        self._cache.clear()
        print(f"[SamplePool] destroy: freed {count} cached blocks")

    def malloc(self, size: int) -> MemBlock:
        bucket = self._cache.get(size)
        if bucket:
            addr = bucket.pop()
            print(f"[SamplePool] reuse : addr=0x{addr:016x}  size={size} B")
            return MemBlock(addr=addr, size=size)
        ptr, ret = acl.rt.malloc(size, ACL_MEM_MALLOC_NORMAL_ONLY)
        if ret != ACL_SUCCESS:
            raise RuntimeError(f"[SamplePool] malloc failed: size={size} B, ret={ret}")
        print(f"[SamplePool] new   : addr=0x{ptr:016x}  size={size} B")
        return MemBlock(addr=ptr, size=size)

    def free(self, block: MemBlock) -> None:
        self._cache.setdefault(block.size, []).append(block.addr)
        print(f"[SamplePool] cache : addr=0x{block.addr:016x}  size={block.size} B")


def run_with_sample_allocator(sample_graph: Graph, session: Session) -> int:
    stream = None
    graph_added = False
    allocator_registered = False
    try:
        # 1. 创建 Stream
        stream, ret = acl.rt.create_stream()
        check_ret("acl.rt.create_stream", ret)

        # 2. 注册自定义 allocator：GE 将通过它为输出 Tensor 申请设备内存
        allocator = SamplePoolAllocator()
        session.register_external_allocator(stream, allocator)
        allocator_registered = True
        print("[Info] SamplePoolAllocator 已注册到 stream")

        # 3. 创建 Device 输入
        inputs = create_input_tensors()

        # 4. 添加并异步执行 Graph
        session.add_graph(GRAPH_ID, sample_graph)
        graph_added = True
        device_outputs = session.run_graph_with_stream_async(GRAPH_ID, stream, list(inputs))

        # 5. 等待 Stream 上的任务完成
        check_ret("acl.rt.synchronize_stream", acl.rt.synchronize_stream(stream))
        print("[Info] 异步执行 Graph 成功！")

        # 6. 将 Device 数据传回 Host 并打印
        host_outputs = [out.to_host() for out in device_outputs]
        for idx, tensor in enumerate(host_outputs, start=1):
            print(f"Tensor{idx} 详情：{tensor}")
        return 0
    finally:
        if allocator_registered:
            session.unregister_external_allocator(stream)
            print("[Info] SamplePoolAllocator 已注销")
        if stream is not None:
            check_ret("acl.rt.destroy_stream", acl.rt.destroy_stream(stream))
        if graph_added and session is not None:
            session.remove_graph(GRAPH_ID)


graph = build_overload_graph()
dump_overload_graph(graph)
run_graph(graph, run_with_sample_allocator)
