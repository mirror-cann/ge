#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""GE public utility APIs."""

import ctypes
from typing import List, Tuple

from ge._capi.pyge_utils_wrapper import ge_utils_lib
from ge.graph import Graph, Node


class GeUtils:
    """Public GE utility APIs."""

    @staticmethod
    def infer_shape(graph: "Graph", input_shapes: List[List[int]]) -> None:
        """Infer shapes for a graph with the given input shapes.

        This API only runs shape inference and does not apply other graph
        optimizations, such as constant folding or dead edge elimination.

        Args:
            graph: Graph object to run shape inference on.
            input_shapes: Input shape list. Each element describes one graph input shape.

        Raises:
            TypeError: If input_shapes is not a list of integer shape lists.
            RuntimeError: If shape inference fails.
        """
        flat_dims, shape_ranks = GeUtils._normalize_input_shapes(input_shapes)
        dims_num = len(flat_dims)
        shape_num = len(shape_ranks)
        dims_arr = (ctypes.c_int64 * dims_num)(*flat_dims) if dims_num > 0 else None
        shape_ranks_arr = (ctypes.c_size_t * shape_num)(*shape_ranks) if shape_num > 0 else None
        ret = ge_utils_lib.GeApiWrapper_GeUtils_InferShape(
            graph._handle, dims_arr, dims_num, shape_ranks_arr, shape_num
        )
        if ret != 0:
            raise RuntimeError("Failed to infer shape")

    @staticmethod
    def check_node_support_on_aicore(node: "Node") -> Tuple[bool, str]:
        """Check whether a node is supported on AICore.

        Args:
            node: Node object to check.

        Returns:
            Tuple of (is_supported, unsupported_reason).

        Raises:
            RuntimeError: If AICore support checking fails.
        """
        is_supported = ctypes.c_bool(False)
        unsupported_reason = ctypes.POINTER(ctypes.c_char)()
        ret = ge_utils_lib.GeApiWrapper_GeUtils_CheckNodeSupportOnAicore(
            node._handle, ctypes.byref(is_supported), ctypes.byref(unsupported_reason)
        )
        try:
            if ret != 0:
                raise RuntimeError("Failed to check node support on AICore")
            reason = ctypes.string_at(unsupported_reason).decode("utf-8") if unsupported_reason else ""
            return bool(is_supported.value), reason
        finally:
            if unsupported_reason:
                ge_utils_lib.GeApiWrapper_GeUtils_FreeString(unsupported_reason)

    @staticmethod
    def _normalize_input_shapes(input_shapes: List[List[int]]) -> Tuple[List[int], List[int]]:
        if not isinstance(input_shapes, list):
            raise TypeError("input_shapes must be a list of shape lists")

        flat_dims: List[int] = []
        shape_ranks: List[int] = []
        for shape in input_shapes:
            if not isinstance(shape, list):
                raise TypeError("input_shapes must be a list of shape lists")
            dims: List[int] = []
            for dim in shape:
                if not isinstance(dim, int):
                    raise TypeError("each shape dim must be an integer")
                dims.append(dim)
            shape_ranks.append(len(dims))
            flat_dims.extend(dims)
        return flat_dims, shape_ranks
