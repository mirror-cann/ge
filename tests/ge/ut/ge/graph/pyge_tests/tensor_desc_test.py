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

"""
TensorDesc 功能测试 - 使用 pytest 框架
测试 tensor_desc.py 中的 Shape 和 TensorDesc 类的各种功能
"""

import copy

import pytest

# 添加 ge 到 Python 路径
try:
    from ge.graph import Shape, TensorDesc
    from ge.graph.types import DataType, Format
except ImportError as e:
    pytest.skip(f"无法导入 ge 模块: {e}", allow_module_level=True)


class TestShape:
    """Shape 功能测试类"""

    @staticmethod
    def test_shape_creation():
        """测试 Shape 创建和列表语义"""
        shape = Shape([1, 3, 224, 224])
        assert isinstance(shape, list)
        assert isinstance(shape, Shape)
        assert shape == [1, 3, 224, 224]
        assert shape[1] == 3

    @staticmethod
    def test_empty_shape():
        """测试空 Shape 表示标量"""
        shape = Shape()
        assert shape == []
        assert shape.get_shape_size() == 0
        assert shape.is_unknown_shape() is False

    @staticmethod
    @pytest.mark.parametrize(
        "shape, expected_size",
        [
            ([2, 3, 4], 24),
            ([1], 1),
            ([0, 3], 0),
            ([2, -1, 4], -1),
            ([-2], -1),
        ],
    )
    def test_get_shape_size(shape, expected_size):
        """测试 Shape 元素数量计算"""
        assert Shape(shape).get_shape_size() == expected_size

    @staticmethod
    @pytest.mark.parametrize(
        "shape, expected",
        [
            ([2, 3, 4], False),
            ([2, -1, 4], True),
            ([-2], True),
        ],
    )
    def test_is_unknown_shape(shape, expected):
        """测试未知 Shape 判断"""
        assert Shape(shape).is_unknown_shape() is expected

    @staticmethod
    @pytest.mark.parametrize(
        "shape",
        [
            "invalid",
            [1, "invalid"],
            [1.0, 2.0],
        ],
    )
    def test_shape_invalid(shape):
        """测试 Shape 参数校验"""
        with pytest.raises(TypeError, match="dims must be a list of integers"):
            Shape(shape)


class TestTensorDesc:
    """TensorDesc 功能测试类"""

    @staticmethod
    def test_tensor_desc_creation():
        """测试 TensorDesc 创建和属性读取"""
        desc = TensorDesc([1, 2, 3], Format.FORMAT_NCHW, DataType.DT_FLOAT)
        shape = desc.get_shape()
        assert isinstance(shape, Shape)
        assert shape == [1, 2, 3]
        assert shape.get_shape_size() == 6
        assert desc.get_format() == Format.FORMAT_NCHW
        assert desc.get_data_type() == DataType.DT_FLOAT

    @staticmethod
    def test_tensor_desc_default_creation():
        """测试 TensorDesc 默认创建"""
        desc = TensorDesc()
        assert desc.shape == []
        assert desc.shape.get_shape_size() == 0
        assert desc.format == Format.FORMAT_ND
        assert desc.data_type == DataType.DT_FLOAT

    @staticmethod
    def test_tensor_desc_setters():
        """测试 TensorDesc setter 和链式调用"""
        desc = TensorDesc([1, 2, 3], Format.FORMAT_NCHW, DataType.DT_FLOAT)
        result = (
            desc.set_shape([2, 3])
            .set_data_type(DataType.DT_INT32)
            .set_format(Format.FORMAT_ND)
            .set_origin_shape([4, 5])
            .set_origin_format(Format.FORMAT_NHWC)
        )

        assert result is desc
        assert desc.shape == [2, 3]
        assert desc.shape.get_shape_size() == 6
        assert desc.data_type == DataType.DT_INT32
        assert desc.format == Format.FORMAT_ND
        assert desc.origin_shape == [4, 5]
        assert desc.origin_format == Format.FORMAT_NHWC

    @staticmethod
    @pytest.mark.parametrize(
        "shape",
        [
            "invalid",
            [1, "invalid"],
            [1.0, 2.0],
        ],
    )
    def test_set_shape_invalid(shape):
        """测试 TensorDesc shape 参数校验"""
        desc = TensorDesc()
        with pytest.raises(TypeError, match="shape must be a list of integers"):
            desc.set_shape(shape)

    @staticmethod
    def test_constructor_invalid():
        """测试 TensorDesc 构造参数校验"""
        with pytest.raises(TypeError, match="Format must be a Format"):
            TensorDesc(format="invalid")
        with pytest.raises(TypeError, match="Data type must be a DataType"):
            TensorDesc(data_type="invalid")
        with pytest.raises(TypeError, match="shape must be a list of integers"):
            TensorDesc(shape="invalid")

    @staticmethod
    def test_set_format_invalid():
        """测试无效 Format 设置"""
        desc = TensorDesc()
        with pytest.raises(TypeError, match="Format must be a Format"):
            desc.set_format("invalid")
        with pytest.raises(TypeError, match="Format must be a Format"):
            desc.set_origin_format("invalid")

    @staticmethod
    def test_set_data_type_invalid():
        """测试无效 DataType 设置"""
        desc = TensorDesc()
        with pytest.raises(TypeError, match="Data type must be a DataType"):
            desc.set_data_type("invalid")

    @staticmethod
    def test_copy_not_supported():
        """测试 TensorDesc 不支持 copy"""
        desc = TensorDesc()
        with pytest.raises(RuntimeError, match="TensorDesc does not support copy"):
            copy.copy(desc)
        with pytest.raises(RuntimeError, match="TensorDesc does not support deepcopy"):
            copy.deepcopy(desc)

    @staticmethod
    def test_create_from_invalid():
        """测试从无效指针创建 TensorDesc"""
        with pytest.raises(ValueError, match="TensorDesc pointer cannot be None"):
            TensorDesc._create_from(None)

    @staticmethod
    def test_repr():
        """测试 TensorDesc 字符串表示"""
        desc = TensorDesc([1, 2], Format.FORMAT_ND, DataType.DT_FLOAT)
        value = repr(desc)
        assert "TensorDesc" in value
        assert "shape=[1, 2]" in value
        assert f"format={Format.FORMAT_ND}" in value
        assert f"data_type={DataType.DT_FLOAT}" in value
