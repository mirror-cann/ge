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

"""Allocator module for external memory allocation in GraphEngine."""

from abc import ABC, abstractmethod


class MemBlock:
    """Represents a block of memory allocated by an Allocator.

    Attributes:
        addr: Device memory address (integer).
        size: Size in bytes.
    """

    def __init__(self, addr: int, size: int):
        self._addr = addr
        self._size = size

    @property
    def addr(self) -> int:
        return self._addr

    @property
    def size(self) -> int:
        return self._size


class Allocator(ABC):
    """Base class for external allocators.

    Subclass this to implement custom device memory allocation strategies.
    The allocator is registered to a stream via Session.register_external_allocator().
    """

    @abstractmethod
    def malloc(self, size: int) -> MemBlock:
        """Allocate device memory of the given size.

        Args:
            size: Number of bytes to allocate.

        Returns:
            A MemBlock whose addr is a valid device memory address.

        Raises:
            MemoryError: If allocation fails.
        """
        raise NotImplementedError

    @abstractmethod
    def free(self, block: MemBlock) -> None:
        """Free memory previously allocated by malloc()."""
        raise NotImplementedError
