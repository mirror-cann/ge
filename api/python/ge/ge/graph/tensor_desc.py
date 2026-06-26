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

"""TensorDesc module for GraphEngine tensor metadata."""

import ctypes
from typing import List, Optional

from ge._capi.pygraph_wrapper import graph_lib

from .types import DataType, Format

UNKNOWN_DIM = -1
UNKNOWN_DIM_NUM = -2
UNKNOWN_DIM_SIZE = -1


class Shape(list):
    """A list subclass representing tensor shape dimensions.

    Extends list with get_shape_size and is_unknown_shape helpers.

    Example:
        >>> shape = Shape([1, 3, 224, 224])
        >>> shape == [1, 3, 224, 224]
        True
        >>> shape.get_shape_size()
        150528
    """

    def __init__(self, dims: Optional[List[int]] = None) -> None:
        """Initialize a Shape.

        Args:
            dims: List of integer dimension values. None means scalar.

        Raises:
            TypeError: If dims is not a list of integers.
        """
        super().__init__(_normalize_dims(dims, "dims"))

    def get_shape_size(self) -> int:
        """Return the total number of elements described by this shape.

        Returns:
            Product of all dimensions, 0 for scalar, or -1 if any dimension is unknown.
        """
        if len(self) == 0:
            return 0
        size = 1
        for dim in self:
            if dim in (UNKNOWN_DIM, UNKNOWN_DIM_NUM):
                return UNKNOWN_DIM_SIZE
            size *= dim
        return size

    def is_unknown_shape(self) -> bool:
        """Check whether the shape contains any unknown dimension.

        Returns:
            True if any dimension is UNKNOWN_DIM (-1) or UNKNOWN_DIM_NUM (-2).
        """
        return any(dim in (UNKNOWN_DIM, UNKNOWN_DIM_NUM) for dim in self)


def _normalize_dims(dims: Optional[List[int]], arg_name: str) -> List[int]:
    """Validate and normalize dims to a plain list of ints."""
    if dims is None:
        return []
    if not isinstance(dims, list) or not all(isinstance(dim, int) for dim in dims):
        raise TypeError(f"{arg_name} must be a list of integers")
    return list(dims)


class TensorDesc:
    """TensorDesc class for GraphEngine tensor metadata.

    This class provides a Pythonic interface for managing tensor metadata
    (shape, format, data type) using the GraphEngine C API.

    Example:
        >>> desc = TensorDesc([1, 3, 224, 224], Format.FORMAT_NCHW, DataType.DT_FLOAT)
        >>> desc.shape
        [1, 3, 224, 224]
        >>> desc.set_shape([2, 3]).set_data_type(DataType.DT_INT32).set_format(Format.FORMAT_ND)
    """

    def __init__(
        self,
        shape: Optional[List[int]] = None,
        format: Optional[Format] = Format.FORMAT_ND,
        data_type: Optional[DataType] = DataType.DT_FLOAT,
    ) -> None:
        """Initialize a TensorDesc.

        Args:
            shape: Shape dimensions. None means scalar.
            format: Data format using Format enum, defaults to Format.FORMAT_ND.
            data_type: Element data type using DataType enum, defaults to DataType.DT_FLOAT.

        Raises:
            TypeError: If format is not a Format or data_type is not a DataType.
            RuntimeError: If TensorDesc creation fails.
        """
        if not isinstance(format, Format):
            raise TypeError("Format must be a Format")
        if not isinstance(data_type, DataType):
            raise TypeError("Data type must be a DataType")

        dims = _normalize_dims(shape, "shape")
        dims_num = len(dims)
        dims_arr = (ctypes.c_int64 * dims_num)(*dims) if dims_num > 0 else None
        self._handle = graph_lib.GeApiWrapper_TensorDesc_Create(dims_arr, dims_num, format, data_type)
        if not self._handle:
            raise RuntimeError("Failed to create TensorDesc")

    def __del__(self) -> None:
        """Clean up resources."""
        if getattr(self, "_handle", None):
            graph_lib.GeApiWrapper_TensorDesc_Destroy(self._handle)
            self._handle = None

    def __copy__(self) -> None:
        """Copy is not supported."""
        raise RuntimeError("TensorDesc does not support copy")

    def __deepcopy__(self, memodict) -> None:
        """Deep copy is not supported."""
        raise RuntimeError("TensorDesc does not support deepcopy")

    def __repr__(self) -> str:
        return f"TensorDesc(shape={self.get_shape()}, format={self.get_format()}, data_type={self.get_data_type()})"

    @property
    def shape(self) -> Shape:
        return self.get_shape()

    @property
    def origin_shape(self) -> Shape:
        return self.get_origin_shape()

    @property
    def format(self) -> Format:
        return self.get_format()

    @property
    def origin_format(self) -> Format:
        return self.get_origin_format()

    @property
    def data_type(self) -> DataType:
        return self.get_data_type()

    @classmethod
    def _create_from(cls, handle: ctypes.c_void_p) -> "TensorDesc":
        """Create TensorDesc object from an existing C++ pointer.

        Takes ownership of the pointer and destroys it on garbage collection.

        Args:
            handle: C++ TensorDesc pointer.

        Returns:
            TensorDesc instance backed by the given handle.

        Raises:
            ValueError: If handle is None.
        """
        if not handle:
            raise ValueError("TensorDesc pointer cannot be None")
        instance = cls.__new__(cls)
        instance._handle = handle
        return instance

    def get_shape(self) -> Shape:
        """Get the shape.

        Returns:
            Shape containing the dimension values.

        Raises:
            RuntimeError: If shape retrieval fails.
        """
        return self._get_shape(graph_lib.GeApiWrapper_TensorDesc_GetShape, "shape")

    def set_shape(self, shape: List[int]) -> "TensorDesc":
        """Set the shape.

        Args:
            shape: List of integer dimension values.

        Returns:
            self, enabling method chaining.

        Raises:
            TypeError: If shape is not a list of integers.
            RuntimeError: If setting shape fails.
        """
        self._set_dims(graph_lib.GeApiWrapper_TensorDesc_SetShape, shape, "shape")
        return self

    def get_origin_shape(self) -> Shape:
        """Get the original shape.

        Returns:
            Shape containing the original dimension values.

        Raises:
            RuntimeError: If origin shape retrieval fails.
        """
        return self._get_shape(graph_lib.GeApiWrapper_TensorDesc_GetOriginShape, "origin shape")

    def set_origin_shape(self, shape: List[int]) -> "TensorDesc":
        """Set the original shape.

        Args:
            shape: List of integer dimension values.

        Returns:
            self, enabling method chaining.

        Raises:
            TypeError: If shape is not a list of integers.
            RuntimeError: If setting origin shape fails.
        """
        self._set_dims(graph_lib.GeApiWrapper_TensorDesc_SetOriginShape, shape, "origin shape")
        return self

    def get_format(self) -> Format:
        """Get the data format.

        Returns:
            Current Format value.
        """
        return Format(graph_lib.GeApiWrapper_TensorDesc_GetFormat(self._handle))

    def set_format(self, format: Format) -> "TensorDesc":
        """Set the data format.

        Args:
            format: Target Format value.

        Returns:
            self, enabling method chaining.

        Raises:
            TypeError: If format is not a Format.
            RuntimeError: If setting format fails.
        """
        if not isinstance(format, Format):
            raise TypeError("Format must be a Format")
        ret = graph_lib.GeApiWrapper_TensorDesc_SetFormat(self._handle, format)
        if ret != 0:
            raise RuntimeError(f"Failed to set format {format}")
        return self

    def get_origin_format(self) -> Format:
        """Get the original data format.

        Returns:
            Original Format value.
        """
        return Format(graph_lib.GeApiWrapper_TensorDesc_GetOriginFormat(self._handle))

    def set_origin_format(self, format: Format) -> "TensorDesc":
        """Set the original data format.

        Args:
            format: Target Format value.

        Returns:
            self, enabling method chaining.

        Raises:
            TypeError: If format is not a Format.
            RuntimeError: If setting origin format fails.
        """
        if not isinstance(format, Format):
            raise TypeError("Format must be a Format")
        ret = graph_lib.GeApiWrapper_TensorDesc_SetOriginFormat(self._handle, format)
        if ret != 0:
            raise RuntimeError(f"Failed to set origin format {format}")
        return self

    def get_data_type(self) -> DataType:
        """Get the element data type.

        Returns:
            DataType value.
        """
        return DataType(graph_lib.GeApiWrapper_TensorDesc_GetDataType(self._handle))

    def set_data_type(self, data_type: DataType) -> "TensorDesc":
        """Set the element data type.

        Args:
            data_type: Target DataType value.

        Returns:
            self, enabling method chaining.

        Raises:
            TypeError: If data_type is not a DataType.
            RuntimeError: If setting data type fails.
        """
        if not isinstance(data_type, DataType):
            raise TypeError("Data type must be a DataType")
        ret = graph_lib.GeApiWrapper_TensorDesc_SetDataType(self._handle, data_type)
        if ret != 0:
            raise RuntimeError(f"Failed to set data type {data_type}")
        return self

    def _get_shape(self, getter, name: str) -> Shape:
        dims_num = ctypes.c_size_t()
        dims_arr = ctypes.POINTER(ctypes.c_int64)()
        ret = getter(self._handle, ctypes.byref(dims_arr), ctypes.byref(dims_num))
        if ret != 0:
            raise RuntimeError(f"Failed to get {name}")
        try:
            return Shape([dims_arr[i] for i in range(dims_num.value)])
        finally:
            graph_lib.GeApiWrapper_Tensor_FreeDimsArray(dims_arr)

    def _set_dims(self, setter, dims: List[int], name: str) -> None:
        normalized = _normalize_dims(dims, name)
        dims_num = len(normalized)
        dims_arr = (ctypes.c_int64 * dims_num)(*normalized) if dims_num > 0 else None
        ret = setter(self._handle, dims_arr, dims_num)
        if ret != 0:
            raise RuntimeError(f"Failed to set {name}")
