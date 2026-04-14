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

import pytest
import ctypes

# 添加 ge 到 Python 路径
try:
    from ge.graph import Tensor
    from ge.graph.tensor import _parse_str_list, unflatten_tensor_data
    from ge.graph.types import DataType, Format, Placement
except ImportError as e:
    pytest.skip(f"无法导入 ge 模块: {e}", allow_module_level=True)

class TestTensor:
    """Tensor 功能测试类"""

    @pytest.fixture
    def tensor(self):
        """创建 Tensor 实例的 fixture"""
        try:
            tensor = Tensor([1, 2, 3, 4, 5, 6], None, DataType.DT_INT8, Format.FORMAT_ND, [1, 2, 3])
            return tensor
        except Exception as e:
            print(f"Exception occurs {e}")

    @staticmethod
    def test_tensor_none_args_init():
        """测试无参初始化"""
        tensor = Tensor()
        assert tensor._handle is not None
        assert tensor.set_data_type(DataType.DT_FLOAT).get_data_type() == DataType.DT_FLOAT


    @staticmethod
    def test_tensor_shape_invalid():
        """测试shape传入错误"""
        with pytest.raises(TypeError, match="Shape must be a list of integers"):  
            tensor = Tensor([1.0, 2.1, 3.2, 4.3, 5.2, 6.0], shape=1)

    @staticmethod
    def test_tensor_placement_invalid():
        with pytest.raises(TypeError, match="Placement must be a Placement"):
            Tensor([1.0], shape=[1], placement=1)

    @staticmethod
    def test_tensor_all_args_invalid():
        """测试所有data和file_path同时传入时错误"""
        with pytest.raises(RuntimeError, match="Tensor should be created either by data or by file"):  
            tensor = Tensor([1.0, 2.1, 3.2, 4.3, 5.2, 6.0], "test.txt")

    @staticmethod
    def test_tensor_data_invalid():
        """测试data类型必须为List"""
        with pytest.raises(TypeError, match="data should be List"):  
            tensor = Tensor(1, shape=[1,2,3])

    @staticmethod
    def test_scalar_tensor_data_invalid():
        """测试data类型必须为List"""
        with pytest.raises(TypeError, match="data should be List"):  
            tensor = Tensor(1)

    @staticmethod
    def test_create_tensor_double_data_invalid():
        """测试通过Double构造Tensor"""
        with pytest.raises(RuntimeError, match="DT_DOUBLE is not supported in python Tensor constructor"):
            tensor = Tensor([1.0, 2.1, 3.2, 4.3, 5.2], data_type=DataType.DT_DOUBLE, shape=[5])

    @staticmethod
    def test_create_tensor_from_file_invalid():
        """测试test.txt文件读取"""
        with pytest.raises(RuntimeError, match="Failed to create Tensor"):  
            tensor = Tensor(file_path="test.txt")

    @staticmethod
    def test_create_tensor_float_data():
        """测试通过Float构造Tensor"""
        tensor = Tensor([1.0, 2.1, 3.2, 4.3, 5.2, 6.0], shape=[1,2,3])
        assert tensor._handle is not None

    @staticmethod
    def test_create_tensor_int_data():
        """测试通过Int构造Tensor"""
        tensor = Tensor([1, 2, 3, 4, 5, 6], data_type=DataType.DT_INT32, shape=[1,2,3])
        assert tensor._handle is not None

    @staticmethod
    def test_create_tensor_bool_data():
        """测试通过Bool构造Tensor"""
        tensor = Tensor([True, True, False, False, False, False], None, DataType.DT_BOOL, Format.FORMAT_ND, [1,2,3])
        assert tensor._handle is not None

    @staticmethod
    def test_create_tensor_invalid_data():
        """测试无效构造Tensor"""
        with pytest.raises(RuntimeError, match="Failed to create Tensor"):  
            tensor = Tensor([1.0, bool, 3.2, 4.3, 5.2, 3], None, DataType.DT_BOOL, Format.FORMAT_ND, [1,2,3])

    @staticmethod
    def test_copy_not_supported(tensor):
        """测试复制不支持"""
        with pytest.raises(RuntimeError, match="Tensor does not support copy"):
            tensor.__copy__()

    @staticmethod
    def test_deepcopy_not_supported(tensor):
        """测试深复制不支持"""
        with pytest.raises(RuntimeError, match="Tensor does not support deepcopy"):
            tensor.__deepcopy__({})
    
    @staticmethod
    def test_tensor_create_from_invalid():
        """测试从无效指针创建 Tensor"""
        with pytest.raises(ValueError, match="Tensor pointer cannot be None"):
            Tensor._create_from(None)

    @staticmethod
    def test_tensor_create_from():
        """测试从有效指针创建 Tensor"""
        tensosr1 = Tensor()
        tensor2 = Tensor._create_from(tensosr1._handle)
        assert tensor2 is not None
        tensor2._owns_handle = False

    @staticmethod
    def test_get_format(tensor):
        """测试获取当前Format"""
        format = tensor.get_format()
        assert format == Format.FORMAT_ND

    @staticmethod
    def test_set_format_invalid(tensor):
        """测试无效Format"""
        with pytest.raises(TypeError, match="Format must be a Format"):
            tensor.set_format("")

    @staticmethod
    def test_set_format_error(tensor):
        """测试set_format错误"""
        handle = tensor._handle
        try:
            with pytest.raises(RuntimeError, match="Failed to set format 0"):
                tensor._handle = ctypes.c_void_p(None)
                tensor.set_format(Format.FORMAT_NCHW)
        finally:
            tensor._handle = handle
        
    @staticmethod
    def test_set_format(tensor):
        """测试set_format功能"""
        # 获取当前Format
        current_format = tensor.get_format()
        assert current_format == Format.FORMAT_ND
        
        # 获取修改后的Format
        tensor.set_format(Format.FORMAT_NCHW)
        current_format = tensor.get_format()
        assert current_format == Format.FORMAT_NCHW

    @staticmethod
    def test_get_data_type(tensor):
        # 获取当前DataType
        current_data_type = tensor.get_data_type()
        assert current_data_type == DataType.DT_INT8

    @staticmethod
    def test_set_data_type(tensor):
        """测试set_data_type功能"""
        # 获取当前DataType
        current_data_type = tensor.get_data_type()
        assert current_data_type == DataType.DT_INT8
        
        # 获取修改后的DataType
        tensor.set_data_type(DataType.DT_INT64)
        current_format = tensor.get_data_type()
        assert current_format == DataType.DT_INT64

    @staticmethod
    def test_set_data_type_invalid(tensor):
        """测试必须传入DataType类型"""
        with pytest.raises(TypeError, match="Data_type must be a DataType"):
            tensor.set_data_type("")

    @staticmethod
    def test_set_data_type_error(tensor):
        """测试必须传入DataType类型"""
        handle = tensor._handle
        try:
            with pytest.raises(RuntimeError, match="Failed to set datatype 9"):
                tensor._handle = ctypes.c_void_p(None)
                tensor.set_data_type(DataType.DT_INT64)
        finally:
            tensor._handle = handle

    @staticmethod
    def test_placement_property(tensor):
        """测试 placement 属性"""
        assert tensor.placement == Placement.PLACEMENT_HOST
        assert tensor.get_placement() == Placement.PLACEMENT_HOST

    @staticmethod
    def test_to_host_on_host_tensor(tensor):
        """测试对 HOST tensor 调用 to_host 触发 ValueError"""
        with pytest.raises(ValueError, match="to_host\\(\\) only supports device tensors"):
            tensor.to_host()

    @staticmethod
    def test_transfer_ownership(tensor):
        """测试Tensor作为Attr时的所有权转移"""
        from ge.es.graph_builder import GraphBuilder
        builder = GraphBuilder()
        tensor = Tensor()
        assert tensor._owner is None
        assert tensor._owns_handle is True
        assert tensor._handle is not None
        # 测试所有权转移, 当tensor作为Attr参数传递给builder时, 所有权转移给builder, 
        tensor._transfer_ownership_when_pass_as_attr(builder)
        assert tensor._owner is not None
        assert tensor._owner == builder
        assert tensor._owns_handle is False
        assert tensor._handle is not None
        with pytest.raises(RuntimeError, match="Tensor already has an new owner builder :graph, cannot transfer ownership again"):
            tensor._transfer_ownership_when_pass_as_attr(builder)
        # 还原为了避免内存泄漏
        tensor._owns_handle = True

    @staticmethod
    def test_get_shape(tensor):
        # 获取当前Shape
        tensor_shape = tensor.get_shape()
        assert len(tensor_shape) == 3
        assert tensor_shape[0] == 1
        assert tensor_shape[1] == 2
        assert tensor_shape[2] == 3

    @staticmethod
    def test_get_data(tensor):
        # 获取当前Data
        tensor_data = tensor.get_data()
        assert len(tensor_data) == 1
        assert len(tensor_data[0]) == 2
        assert len(tensor_data[0][0]) == 3

    @staticmethod
    def test_get_data_error(tensor):
        # 获取当前Data
        handle = tensor._handle
        try:
            with pytest.raises(RuntimeError, match="Failed to get Tensor data"):
                tensor._handle = ctypes.c_void_p(None)
                tensor_data = tensor.get_data()
        finally:
            tensor._handle = handle

    @staticmethod
    def test_tensor_print(tensor):
        """测试Tensor打印功能"""
        val = tensor.__str__()
        print(tensor)
        assert val == f'''
        Tensor format is 2, 
        data_type is 2, 
        shape is [1, 2, 3], 
        data is [[[1, 2, 3], [4, 5, 6]]]
        '''

    @staticmethod
    def test_parse_str_list_invalid(tensor):
        """验证解析无效 list字符串"""
        with pytest.raises(ValueError, match="Input must start with '[' and end with ']'"):
            _parse_str_list("")

    @staticmethod
    def test_parse_str_list_empty(tensor):
        """验证解析空 list字符串"""
        res = _parse_str_list("[]")
        assert len(res) == 0

    @staticmethod
    def test_parse_str_list_int(tensor):
        """验证解析int list字符串"""
        res = _parse_str_list("[1, 2, -3, 5, 7]")
        assert len(res) == 5
        assert res[2] == -3

    @staticmethod
    def test_parse_str_list_float(tensor):
        """验证解析float list字符串"""
        res = _parse_str_list("[1.0, 2.0, -3.2, 5.7, 7.22, 9.0]")
        assert len(res) == 6
        assert res[2] == -3.2


    @staticmethod
    def test_parse_str_list_mix(tensor):
        """验证解析list字符串无效"""
        res = _parse_str_list("[1, 2.0, 3.3, 5, 7]")
        assert len(res) == 5
        assert res[2] == 3.3

    @staticmethod
    def test_parse_str_list_invalid(tensor):
        """验证解析list字符串无效"""
        with pytest.raises(ValueError, match="Invalid item: '2;23'"):
            res = _parse_str_list("[1, 2;23 , 3.3, 5, 7]")

    @staticmethod
    def test_unflatten_tensor_data(tensor):
        """验证正常TensorData去扁平化"""
        res = unflatten_tensor_data("[1.0, 2.0, 3.0, 4.0, 5.0, 6.0]", [3, 2, 1])
        assert res.__str__() == "[[[1.0], [2.0]], [[3.0], [4.0]], [[5.0], [6.0]]]"
