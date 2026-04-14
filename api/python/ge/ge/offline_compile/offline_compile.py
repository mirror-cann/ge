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

"""Offline graph compilation module."""

import ctypes
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from ge._capi.pyoffline_compile_wrapper import ModelBufferDataPtr, offline_compile_lib
from ge.graph.graph import Graph


@dataclass
class GraphWithOptions:
    """One graph together with its build options for bundle compilation."""

    graph: Graph
    build_options: Dict[str, str] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if not isinstance(self.graph, Graph):
            raise TypeError("graph must be a Graph")
        self.build_options = _normalize_options(self.build_options, "build_options")


class ModelBuffer:
    """ModelBuffer class for offline build operations.

    This class provides a Pythonic interface for offline build operations
    using the GraphEngine C API.

    Example:
        >>> model = build_model(graph, {"input_format": "ND"})
        >>> size = model.length
        >>> save_model("sample", model)
    """

    def __init__(self) -> None:
        """Prevent direct instantiation of ModelBuffer objects."""
        self._owns_handle = False
        self._handle = None
        raise RuntimeError("ModelBuffer objects should not be created directly")

    def __del__(self) -> None:
        """Clean up resources."""
        if self._owns_handle:
            offline_compile_lib.GeApiWrapper_ModelBuffer_Destroy(self._handle)
            self._handle = None

    def __copy__(self) -> None:
        """Copy is not supported."""
        raise RuntimeError("ModelBuffer does not support copy")

    def __deepcopy__(self, memodict) -> None:
        """Deep copy is not supported."""
        raise RuntimeError("ModelBuffer does not support deepcopy")

    @property
    def length(self) -> int:
        """Get model buffer length.

        Returns:
            Model buffer length in bytes.
        """
        return self.get_length()

    @classmethod
    def _create_from(cls, handle: ModelBufferDataPtr) -> "ModelBuffer":
        """Create ModelBuffer object from C++ handle. (internal use only by e.g
         build_model(graph, build_options), do not use this method directly)

        Args:
            handle: C++ ModelBufferData object handle.

        Returns:
            ModelBuffer object.

        Raises:
            ValueError: If handle is None.
        """
        if not handle:
            raise ValueError("Failed to create ModelBuffer")
        instance = cls.__new__(cls)
        instance._handle = handle
        instance._owns_handle = True
        return instance

    def get_length(self) -> int:
        """Get model buffer length.

        Returns:
            Model buffer length in bytes.
        """
        return int(offline_compile_lib.GeApiWrapper_ModelBuffer_GetLength(self._handle))


def _normalize_bundle_options(graph_with_options: List[GraphWithOptions]) -> List[GraphWithOptions]:
    if not isinstance(graph_with_options, list):
        raise TypeError("graph_with_options must be a list")
    if len(graph_with_options) <= 1:
        raise ValueError("graph_with_options size must be larger than 1")

    normalized_items = []
    for item in graph_with_options:
        if not isinstance(item, GraphWithOptions):
            raise TypeError("Each item in graph_with_options must be a GraphWithOptions")
        normalized_items.append(item)
    return normalized_items


def _normalize_options(options: Optional[dict], arg_name: str) -> Dict[str, str]:
    if options is None:
        return {}
    if not isinstance(options, dict):
        raise TypeError(f"{arg_name} must be a dictionary")
    normalized = {}
    for key, value in options.items():
        if not isinstance(key, str) or not isinstance(value, str):
            raise TypeError(f"{arg_name} keys and values must be strings")
        normalized[key] = value
    return normalized


def _dict_to_c_arrays(options: Dict[str, str]) -> Tuple[Optional[ctypes.Array], Optional[ctypes.Array], int]:
    if not options:
        return None, None, 0

    size = len(options)
    key_array = (ctypes.c_char_p * size)()
    value_array = (ctypes.c_char_p * size)()
    for index, (key, value) in enumerate(options.items()):
        key_array[index] = key.encode("utf-8")
        value_array[index] = value.encode("utf-8")
    return key_array, value_array, size


def _cast_char_array(array: Optional[ctypes.Array]):
    if array is None:
        return None
    return ctypes.cast(array, ctypes.POINTER(ctypes.c_char_p))


def build_initialize(global_options: Optional[dict] = None) -> None:
    """Initialize resources required for offline graph build.

    Args:
        global_options: Optional global build configuration map. Keys and
            values must both be strings.

    Raises:
        TypeError: If arguments have incorrect types.
        RuntimeError: If GE fails to initialize build resources.
    """
    options = _normalize_options(global_options, "global_options")
    key_array, value_array, size = _dict_to_c_arrays(options)
    ret = offline_compile_lib.GeApiWrapper_OfflineCompile_BuildInitialize(
        _cast_char_array(key_array),
        _cast_char_array(value_array),
        size,
    )
    if ret != 0:
        raise RuntimeError("Failed to initialize aclgrph build")


def build_finalize() -> None:
    """Release all resources."""
    offline_compile_lib.GeApiWrapper_OfflineCompile_BuildFinalize()


def build_model(graph: Graph, build_options: Optional[dict] = None) -> ModelBuffer:
    """Compile a graph into an offline model kept in memory.

    Args:
        graph: The Graph object to compile.
        build_options: Optional graph-level build configuration map. Keys and
            values must both be strings. If an option is configured both here
            and in func:`build_initialize`, the value passed here takes
            precedence.

    Returns:
        ModelBuffer containing the compiled offline model.

    Raises:
        TypeError: If arguments have incorrect types.
        RuntimeError: If GE fails to compile the graph.
    """
    if not isinstance(graph, Graph):
        raise TypeError("graph must be a Graph")
    options = _normalize_options(build_options, "build_options")
    key_array, value_array, size = _dict_to_c_arrays(options)
    model_buffer_handle = ModelBufferDataPtr()
    ret = offline_compile_lib.GeApiWrapper_OfflineCompile_BuildModel(
        graph._handle,
        _cast_char_array(key_array),
        _cast_char_array(value_array),
        size,
        ctypes.byref(model_buffer_handle),
    )
    if ret != 0:
        raise RuntimeError("Failed to build model")
    return ModelBuffer._create_from(model_buffer_handle)


def save_model(output_file: str, model: ModelBuffer) -> None:
    """Serialize an in-memory offline model to an ``.om`` file.

    Args:
        output_file: Base name of the output model file. The generated offline
            model file name automatically ends with the ``.om`` suffix. If the
            OM file name contains the operating system and architecture, the
            OM file can only be used in the runtime environment for that
            operating system and architecture.
        model: Offline model buffer.

    Raises:
        TypeError: If arguments have incorrect types.
        RuntimeError: If GE fails to save the model.
    """
    if not isinstance(output_file, str):
        raise TypeError("output_file must be a string")
    if not isinstance(model, ModelBuffer):
        raise TypeError("model must be a ModelBuffer")
    ret = offline_compile_lib.GeApiWrapper_OfflineCompile_SaveModel(output_file.encode("utf-8"), model._handle)
    if ret != 0:
        raise RuntimeError("Failed to save model")


def bundle_build_model(graph_with_options: List[GraphWithOptions]) -> ModelBuffer:
    """Compile a group of graphs into a bundle offline model.

    Args:
        graph_with_options: A list of GraphWithOptions items to
            compile together. Each item contains one graph and its build
            options. The list must contain more than one graph.

    Returns:
        ModelBuffer containing the compiled bundle model.

    Raises:
        TypeError: If arguments have incorrect types.
        ValueError: If fewer than two graphs are provided.
        RuntimeError: If GE fails to compile the bundle model.
    """
    normalized_items = _normalize_bundle_options(graph_with_options)
    graph_count = len(normalized_items)
    graph_handles = (ctypes.c_void_p * graph_count)(*[item.graph._handle for item in normalized_items])
    size_array = (ctypes.c_int * graph_count)()
    # Hold references to each graph's option ctypes arrays so pointers in keys/values stay valid for the C call.
    option_buffers: List[Tuple[Optional[ctypes.Array], Optional[ctypes.Array]]] = []
    keys = (ctypes.POINTER(ctypes.c_char_p) * graph_count)()
    values = (ctypes.POINTER(ctypes.c_char_p) * graph_count)()
    for index, item in enumerate(normalized_items):
        key_array, value_array, size = _dict_to_c_arrays(item.build_options)
        option_buffers.append((key_array, value_array))
        keys[index] = _cast_char_array(key_array)
        values[index] = _cast_char_array(value_array)
        size_array[index] = size

    model_buffer_handle = ModelBufferDataPtr()
    ret = offline_compile_lib.GeApiWrapper_OfflineCompile_BundleBuildModel(
        graph_handles,
        keys,
        values,
        size_array,
        graph_count,
        ctypes.byref(model_buffer_handle),
    )
    if ret != 0:
        raise RuntimeError("Failed to build bundle model")
    return ModelBuffer._create_from(model_buffer_handle)


def bundle_save_model(output_file: str, model: ModelBuffer) -> None:
    """Serialize an in-memory bundle model to an ``.om`` file.

    Args:
        output_file: Base name of the output model file. The generated offline
            model file name automatically ends with the ``.om`` suffix. If the
            OM file name contains the operating system and architecture, the
            OM file can only be used in the runtime environment for that
            operating system and architecture.
        model: Bundle model buffer.

    Raises:
        TypeError: If arguments have incorrect types.
        RuntimeError: If GE fails to save the bundle model.
    """
    if not isinstance(output_file, str):
        raise TypeError("output_file must be a string")
    if not isinstance(model, ModelBuffer):
        raise TypeError("model must be a ModelBuffer")
    ret = offline_compile_lib.GeApiWrapper_OfflineCompile_BundleSaveModel(
        output_file.encode("utf-8"),
        model._handle
    )
    if ret != 0:
        raise RuntimeError("Failed to save bundle model")
