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

import ctypes
import os

from ._lib_loader import load_lib_from_path

LIB_NAME = "libgraph_wrapper.so"
_dir = os.path.dirname(os.path.abspath(__file__))
offline_compile_lib = load_lib_from_path(LIB_NAME, _dir)

# 常用C类型别名
c_void_p = ctypes.c_void_p
c_char_p = ctypes.c_char_p
c_int = ctypes.c_int
c_uint64 = ctypes.c_uint64
c_p_to_char_p = ctypes.POINTER(c_char_p)
c_pp_to_char_p = ctypes.POINTER(c_p_to_char_p)
c_p_to_void_p = ctypes.POINTER(c_void_p)
c_p_to_int = ctypes.POINTER(c_int)


class ModelBufferData(ctypes.Structure):
    """C层 struct ModelBufferData"""
    pass


# 指针类型定义
ModelBufferDataPtr = ctypes.POINTER(ModelBufferData)

# ============ Offline compile C API ============

offline_compile_lib.GeApiWrapper_OfflineCompile_BuildInitialize.argtypes = [c_p_to_char_p, c_p_to_char_p, c_int]
offline_compile_lib.GeApiWrapper_OfflineCompile_BuildInitialize.restype = c_int

offline_compile_lib.GeApiWrapper_OfflineCompile_BuildFinalize.argtypes = []
offline_compile_lib.GeApiWrapper_OfflineCompile_BuildFinalize.restype = None

offline_compile_lib.GeApiWrapper_OfflineCompile_BuildModel.argtypes = [
    c_void_p, c_p_to_char_p, c_p_to_char_p, c_int, ctypes.POINTER(ModelBufferDataPtr),
]
offline_compile_lib.GeApiWrapper_OfflineCompile_BuildModel.restype = c_int

offline_compile_lib.GeApiWrapper_OfflineCompile_SaveModel.argtypes = [c_char_p, ModelBufferDataPtr]
offline_compile_lib.GeApiWrapper_OfflineCompile_SaveModel.restype = c_int

offline_compile_lib.GeApiWrapper_OfflineCompile_BundleBuildModel.argtypes = [
    c_p_to_void_p, c_pp_to_char_p, c_pp_to_char_p, c_p_to_int, c_int, ctypes.POINTER(ModelBufferDataPtr),
]
offline_compile_lib.GeApiWrapper_OfflineCompile_BundleBuildModel.restype = c_int

offline_compile_lib.GeApiWrapper_OfflineCompile_BundleSaveModel.argtypes = [c_char_p, ModelBufferDataPtr]
offline_compile_lib.GeApiWrapper_OfflineCompile_BundleSaveModel.restype = c_int

offline_compile_lib.GeApiWrapper_ModelBuffer_Destroy.argtypes = [ModelBufferDataPtr]
offline_compile_lib.GeApiWrapper_ModelBuffer_Destroy.restype = None

offline_compile_lib.GeApiWrapper_ModelBuffer_GetLength.argtypes = [ModelBufferDataPtr]
offline_compile_lib.GeApiWrapper_ModelBuffer_GetLength.restype = c_uint64


def get_offline_compile_lib():
    """Get the offline compile wrapper library handle.

    Returns:
        ctypes.CDLL: The loaded libgraph_wrapper.so library handle.
    """
    return offline_compile_lib


def is_offline_compile_lib_loaded():
    """Check if the offline compile library is loaded.

    Returns:
        bool: True if library is loaded successfully.
    """
    return offline_compile_lib is not None
