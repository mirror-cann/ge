# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

from __future__ import annotations

from typing import List, overload

from ge.graph.types import DataType, Format

__all__: List[str] = [
    "ExpandDimsType",
    "Shape",
    "StorageFormat",
    "StorageShape",
    "Tensor",
    "TensorPlacement",
]


class TensorPlacement:
    """Placement enum for runtime Tensor data.

    Describes where tensor data is stored: device HBM, host memory, data
    following the tensor object, or device P2P memory.
    """

    ON_DEVICE_HBM: "TensorPlacement"
    ON_HOST: "TensorPlacement"
    FOLLOWING: "TensorPlacement"
    ON_DEVICE_P2P: "TensorPlacement"
    END: "TensorPlacement"


class Shape:
    """Runtime shape represented by dimension values.

    A ``Shape`` stores dimension values. ``dim_num == 0`` means scalar.

    **Constraints**

    - ``dims`` length must not exceed the maximum supported dimension count.
    - ``get_dim(index)`` / ``set_dim(index, value)`` require
      ``index < dim_num``; otherwise ``ValueError`` is raised.
    """

    @property
    def dims(self) -> List[int]:
        """Return all current dimension values as a list."""
        ...

    @property
    def dim_num(self) -> int:
        """Return the number of dimensions."""
        ...

    @property
    def size(self) -> int:
        """Return the element count of this shape."""
        ...

    def is_scalar(self) -> bool:
        """Return whether this shape is scalar (``dim_num == 0``)."""
        ...

    def get_dim(self, index: int) -> int:
        """Return the dimension value at ``index``."""
        ...

    def set_dim(self, index: int, value: int) -> None:
        """Set the existing dimension value at ``index``."""
        ...

    def append_dim(self, value: int) -> "Shape":
        """Append one dimension value and return ``self``.

        If the maximum dimension count has already been reached, this call
        leaves the shape unchanged.
        """
        ...

    def set_scalar(self) -> None:
        """Set this shape to scalar by clearing all dimensions."""
        ...


class StorageShape:
    """Origin and runtime shapes for Tensor metadata.

    ``origin_shape`` is the original logical shape. ``storage_shape`` is the
    runtime/storage shape used by GE execution.
    """

    @overload
    def __init__(self) -> None:
        """Construct with default scalar origin and storage shapes."""
        ...

    @overload
    def __init__(self, origin_shape: List[int], storage_shape: List[int]) -> None:
        """Construct from origin-shape dimensions and storage-shape dimensions."""
        ...

    @property
    def origin_shape(self) -> Shape:
        """Return the original logical shape."""
        ...

    @property
    def storage_shape(self) -> Shape:
        """Return the runtime/storage shape."""
        ...

    def set_origin_shape(self, shape: Shape) -> None:
        """Replace the original logical shape with ``shape``."""
        ...

    def set_storage_shape(self, shape: Shape) -> None:
        """Replace the runtime/storage shape with ``shape``."""
        ...


class ExpandDimsType:
    """Expand-dimension rule.

    The rule marks which axes in the expanded shape are inserted dimensions.

    **Constraints**

    - String rules longer than the maximum supported expand size are ignored by
      the constructor.
    - Integer rules use the bit-field encoding: high 8 bits store the rule
      length, low 56 bits store the expand mask in reverse string order.
    """

    @overload
    def __init__(self) -> None:
        """Construct an empty expand-dimension rule."""
        ...

    @overload
    def __init__(self, rule: str) -> None:
        """Construct from a string rule such as ``"101"``."""
        ...

    @overload
    def __init__(self, rule: int) -> None:
        """Construct from the integer bit-field rule."""
        ...

    @property
    def full_size(self) -> int:
        """Return the dimension count after expansion."""
        ...

    def is_expand_index(self, index: int) -> bool:
        """Return whether ``index`` is an inserted dimension in the expanded shape."""
        ...

    def set_expand_index(self, index: int) -> None:
        """Mark ``index`` as an inserted dimension."""
        ...


class StorageFormat:
    """Origin and runtime formats for Tensor metadata.

    A storage format records original format, runtime/storage format, and the
    expand-dimension rule used between them.
    """

    @overload
    def __init__(self) -> None:
        """Construct with ``Format.ND`` as origin and storage format."""
        ...

    @overload
    def __init__(
        self,
        origin_format: Format,
        storage_format: Format,
        expand_dims_type: ExpandDimsType,
    ) -> None:
        """Construct from original format, runtime/storage format, and expand rule."""
        ...

    @property
    def origin_format(self) -> Format:
        """Return the original format."""
        ...

    @property
    def storage_format(self) -> Format:
        """Return the runtime/storage format."""
        ...

    @property
    def expand_dims_type(self) -> ExpandDimsType:
        """Return the expand-dimension rule."""
        ...

    def set_origin_format(self, value: Format) -> None:
        """Set the original format."""
        ...

    def set_storage_format(self, value: Format) -> None:
        """Set the runtime/storage format."""
        ...

    def set_expand_dims_type(self, value: ExpandDimsType) -> None:
        """Set the expand-dimension rule."""
        ...


class Tensor:
    """Runtime tensor metadata.

    ``Tensor`` exposes shape, format, placement, data type, data address, and
    byte-size metadata. It does not allocate or copy tensor data for Python.
    """

    @property
    def addr(self) -> int:
        """Return the tensor data address as an integer."""
        ...

    @property
    def size(self) -> int:
        """Return the tensor memory size in bytes."""
        ...

    @property
    def shape_size(self) -> int:
        """Return the element count from the storage shape."""
        ...

    @property
    def shape(self) -> StorageShape:
        """Return the combined origin/storage shape metadata."""
        ...

    @property
    def origin_shape(self) -> Shape:
        """Return the original logical shape."""
        ...

    @property
    def storage_shape(self) -> Shape:
        """Return the runtime/storage shape."""
        ...

    @property
    def format(self) -> StorageFormat:
        """Return the combined origin/storage format metadata."""
        ...

    @property
    def storage_format(self) -> Format:
        """Return the runtime/storage format."""
        ...

    @property
    def origin_format(self) -> Format:
        """Return the original format."""
        ...

    @property
    def expand_dims_type(self) -> ExpandDimsType:
        """Return the expand-dimension rule from the tensor format."""
        ...

    @property
    def data_type(self) -> DataType:
        """Return the tensor data type."""
        ...

    @property
    def placement(self) -> TensorPlacement:
        """Return the tensor data placement."""
        ...
