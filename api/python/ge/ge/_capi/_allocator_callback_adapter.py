#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""Private adapter for wiring Python Allocator objects to C callbacks."""

from typing import Dict

from ge.allocator import Allocator, MemBlock

from .pysession_wrapper import (
    c_func_t_free,
    c_func_t_get_addr,
    c_func_t_malloc,
    c_func_t_on_destroy,
)

# 防止 Python 回调对象被 GC 回收，C++ 析构时回调清理
_prevent_gc: Dict[int, "_AllocatorCCallbacks"] = {}


class _AllocatorCCallbacks:
    """Adapters a Python Allocator to C function-pointer callbacks via ctypes."""

    def __init__(self, allocator: Allocator):
        self._allocator = allocator
        # 持有引用防止 GC 回收；C++ 回调 free 时释放。
        self._blocks: Dict[int, MemBlock] = {}

        def _malloc(_py_obj, size):
            blk = self._allocator.malloc(size)
            key = id(blk)
            self._blocks[key] = blk
            return key

        def _free(_py_obj, block_key):
            blk = self._blocks.pop(block_key, None)
            if blk is not None:
                self._allocator.free(blk)

        def _get_addr(block_key):
            blk = self._blocks.get(block_key)
            return blk.addr if blk else 0

        self.c_malloc = c_func_t_malloc(_malloc)
        self.c_free = c_func_t_free(_free)
        self.c_get_addr = c_func_t_get_addr(_get_addr)


def _on_allocator_destroy(prevent_gc_key):
    """Called from C++ ~PyCallbackAllocator to release Python callback refs."""
    cb = _prevent_gc.pop(prevent_gc_key, None)
    if cb is not None:
        cb.c_malloc = None
        cb.c_free = None
        cb.c_get_addr = None


_c_on_allocator_destroy = c_func_t_on_destroy(_on_allocator_destroy)


def create_allocator_c_callbacks(allocator: Allocator):
    """Create and retain ctypes callbacks for a Python allocator."""
    cb = _AllocatorCCallbacks(allocator)
    prevent_gc_key = id(cb)
    _prevent_gc[prevent_gc_key] = cb
    return cb, prevent_gc_key, _c_on_allocator_destroy


def rollback_allocator_c_callbacks(prevent_gc_key: int) -> None:
    """Rollback retained ctypes callbacks when C++ registration fails."""
    _prevent_gc.pop(prevent_gc_key, None)
