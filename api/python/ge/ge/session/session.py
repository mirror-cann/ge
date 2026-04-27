#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# -------------------------------------------------------------------
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

"""Session module for GraphEngine graph operations."""

import ctypes
import threading
import weakref
from typing import List, Optional
from ge._capi._allocator_callback_adapter import create_allocator_c_callbacks, rollback_allocator_c_callbacks
from ge._capi.pysession_wrapper import session_lib
from ge.allocator import Allocator
from ge.graph.graph import Graph
from ge.graph.tensor import Tensor


_default_allocator_lock = threading.RLock()
# stream -> weakref(session) that currently holds the default allocator for that stream.
_stream_to_default_allocator_owner: dict = {}


def _is_default_allocator_owner(session, stream: int) -> bool:
    owner_ref = _stream_to_default_allocator_owner.get(stream)
    if owner_ref is None:
        return False

    owner = owner_ref()
    if owner is None:
        _stream_to_default_allocator_owner.pop(stream, None)
        return False

    return owner is session


def _str_list_to_c_array(python_list: list):
    """
        Convert python list to c array

        Parameters:
            python_list: python list.

        Returns:
            c_array: c array_ptr.
    """
    size = len(python_list)
    c_array = (ctypes.c_char_p * size)()
    for i, item in enumerate(python_list):
        c_array[i] = item.encode('utf-8')
    return c_array


def _tensor_list_to_c_array(tensors: List[Tensor]):
    handles = [ctypes.cast(tensor._handle, ctypes.c_void_p) for tensor in tensors]
    arr_type = ctypes.c_void_p * len(handles)
    return arr_type(*handles)


class Session:
    """Session class for session operations.

    This class provides a Pythonic interface for session operations
    using the session C API.
    """

    def __init__(self, options: Optional[dict] = None) -> None:
        """Session Initialize a Session"""
        self._handle = None
        self._owns_handle = False
        self._default_allocator_streams = set()
        if options is None:
            self._handle = session_lib.GeApiWrapper_Session_CreateSession()
        elif isinstance(options, dict):
            keys = [k for k in options.keys()]
            values = [v for v in options.values()]
            c_array_key = _str_list_to_c_array(keys)
            c_array_value = _str_list_to_c_array(values)
            c_array_key_ptr = ctypes.cast(c_array_key, ctypes.POINTER(ctypes.c_char_p))
            c_value_key_ptr = ctypes.cast(c_array_value, ctypes.POINTER(ctypes.c_char_p))
            self._handle = session_lib.GeApiWrapper_Session_CreateSessionWithOptions(c_array_key_ptr, c_value_key_ptr,
                                                                                     len(keys))
        else:
            raise TypeError("option must be a dictionary")
        if not self._handle:
            raise RuntimeError("Failed to create session")
        self._owns_handle = True

    def __del__(self) -> None:
        """Clean up resources."""
        self._unregister_default_allocators()
        self._default_allocator_streams.clear()
        if self._owns_handle:
            session_lib.GeApiWrapper_Session_DestroySession(self._handle)
            self._handle = None

    def __copy__(self) -> None:
        """Copy is not supported."""
        raise RuntimeError("Session does not support copy")

    def __deepcopy__(self, session) -> None:
        """Deep copy is not supported."""
        raise RuntimeError("Session does not support deepcopy")

    def register_external_allocator(self, stream: int, allocator: Allocator) -> None:
        """Register an external allocator for the given stream.

        Args:
            stream: Stream address.
            allocator: An Allocator subclass instance.
        """
        if not isinstance(stream, int):
            raise TypeError("stream must be an integer")
        if not isinstance(allocator, Allocator):
            raise TypeError("allocator must be an Allocator instance")
        cb, prevent_gc_key, c_on_allocator_destroy = create_allocator_c_callbacks(allocator)
        with _default_allocator_lock:
            ret = session_lib.GeApiWrapper_Session_RegisterExternalAllocator(
                self._handle, ctypes.c_void_p(stream),
                cb.c_malloc, cb.c_free, cb.c_get_addr,
                c_on_allocator_destroy, ctypes.c_void_p(prevent_gc_key)
            )
            if ret == 0:
                self._default_allocator_streams.discard(stream)
                _stream_to_default_allocator_owner.pop(stream, None)
        if ret != 0:
            rollback_allocator_c_callbacks(prevent_gc_key)
            raise RuntimeError(f"Failed to register allocator for stream 0x{stream:x}")

    def unregister_external_allocator(self, stream: int) -> None:
        """Unregister the external allocator for the given stream.

        Args:
            stream: Stream address.
        """
        if not isinstance(stream, int):
            raise TypeError("stream must be an integer")
        with _default_allocator_lock:
            ret = session_lib.GeApiWrapper_Session_UnregisterExternalAllocator(
                self._handle, ctypes.c_void_p(stream))
            if ret == 0:
                self._default_allocator_streams.discard(stream)
                _stream_to_default_allocator_owner.pop(stream, None)
        if ret != 0:
            raise RuntimeError(f"Failed to unregister allocator for stream 0x{stream:x}")

    def add_graph(self, graph_id: int, add_graph: Graph, options: dict = None) -> None:
        if not isinstance(graph_id, int):
            raise TypeError("Graph_id must be an integer")
        if not isinstance(add_graph, Graph):
            raise TypeError("Add_graph must be a Graph")
        if options is None:
            ret = session_lib.GeApiWrapper_Session_AddGraph(self._handle, ctypes.c_uint32(graph_id), add_graph._handle)
        elif not isinstance(options, dict):
            raise TypeError("options must be a dictionary")
        else:
            keys = [k for k in options.keys()]
            values = [v for v in options.values()]
            c_array_key = _str_list_to_c_array(keys)
            c_array_value = _str_list_to_c_array(values)
            c_array_key_ptr = ctypes.cast(c_array_key, ctypes.POINTER(ctypes.c_char_p))
            c_value_key_ptr = ctypes.cast(c_array_value, ctypes.POINTER(ctypes.c_char_p))
            ret = session_lib.GeApiWrapper_Session_AddGraphWithOptions(self._handle, ctypes.c_uint32(graph_id),
                                                                       add_graph._handle,
                                                                       c_array_key_ptr, c_value_key_ptr, len(keys))
        if ret != 0:
            raise RuntimeError(f"Failed to add graph, graph_id is {graph_id}")
        return ret

    def remove_graph(self, graph_id: int) -> None:
        if not isinstance(graph_id, int):
            raise TypeError("Graph_id must be an integer")
        ret = session_lib.GeApiWrapper_Session_RemoveGraph(self._handle, ctypes.c_uint32(graph_id))
        if ret != 0:
            raise RuntimeError(f"Failed to remove graph, graph_id is {graph_id}")

    def run_graph(self, graph_id: int, inputs: List[Tensor]) -> List[Tensor]:
        if not isinstance(graph_id, int):
            raise TypeError("Graph_id must be an integer")
        if not all(isinstance(input_tensor, Tensor) for input_tensor in inputs):
            raise TypeError("All elements in inputs must be the type of Tensor")
        arr = _tensor_list_to_c_array(inputs)
        tensor_num = ctypes.c_size_t()
        output_tensors = session_lib.GeApiWrapper_Session_RunGraph(self._handle, ctypes.c_uint32(graph_id), arr,
                                                                   len(inputs), ctypes.byref(tensor_num))
        if not output_tensors:
            raise RuntimeError(f"Failed to run graph, graph_id is {graph_id}")
        return [Tensor._create_from(output_tensors[i]) for i in range(tensor_num.value)]

    def run_graph_with_stream_async(self, graph_id: int, stream: int, inputs: List[Tensor]) -> List[Tensor]:
        """Run the graph asynchronously on the given stream and return output tensors.

        Output tensor memory is allocated according to the following priority:
          1. The external allocator registered via register_external_allocator(stream, allocator).
          2. If no external allocator is registered, GE uses a built-in allocator automatically.

        Args:
            graph_id: Graph ID.
            stream: Stream address.
            inputs: List of input tensors.

        Returns:
            List of output tensors.
        """
        if not isinstance(graph_id, int):
            raise TypeError("Graph_id must be an integer")
        if not isinstance(stream, int):
            raise TypeError("Stream must be an integer")
        if not isinstance(inputs, list):
            raise TypeError("inputs must be a list of Tensor")
        if not all(isinstance(input_tensor, Tensor) for input_tensor in inputs):
            raise TypeError("All elements in inputs must be the type of Tensor")

        self._ensure_default_allocator(stream)
        arr = _tensor_list_to_c_array(inputs)
        tensor_num = ctypes.c_size_t()
        output_tensors = session_lib.GeApiWrapper_Session_RunGraphWithStreamAsync(self._handle,
            ctypes.c_uint32(graph_id), ctypes.c_void_p(stream), arr, len(inputs), ctypes.byref(tensor_num)
        )
        if not output_tensors:
            raise RuntimeError(f"Failed to run graph with stream async, graph_id is {graph_id}")
        return [Tensor._create_from(output_tensors[i]) for i in range(tensor_num.value)]

    def _ensure_default_allocator(self, stream: int) -> None:
        """Register a default allocator if none exists."""
        with _default_allocator_lock:
            has_external = session_lib.GeApiWrapper_HasExternalAllocator(ctypes.c_void_p(stream))
            if stream in self._default_allocator_streams or has_external:
                return
            ret = session_lib.GeApiWrapper_Session_RegisterDefaultAllocator(
                self._handle, ctypes.c_void_p(stream))
            if ret == 0:
                self._default_allocator_streams.add(stream)
                _stream_to_default_allocator_owner[stream] = weakref.ref(self)
        if ret != 0:
            raise RuntimeError(f"Failed to register default allocator for stream {stream}")

    def _unregister_default_allocators(self) -> None:
        if not self._default_allocator_streams or not session_lib.GeApiWrapper_IsGEInitialized():
            return

        with _default_allocator_lock:
            for stream in list(self._default_allocator_streams):
                if not _is_default_allocator_owner(self, stream):
                    continue
                if not session_lib.GeApiWrapper_HasDefaultAllocator(ctypes.c_void_p(stream)):
                    _stream_to_default_allocator_owner.pop(stream, None)
                    continue

                session_lib.GeApiWrapper_Session_UnregisterExternalAllocator(self._handle, ctypes.c_void_p(stream))
                _stream_to_default_allocator_owner.pop(stream, None)