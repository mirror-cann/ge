/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "attribute_group/attr_group_base.h"

namespace af {
class AscTensorAttr;
class AscNodeAttr;
class AscGraphAttr;
class AutoFuseAttrs;
class AutoFuseGraphAttrs;
}  // namespace af

namespace ge {
template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<AttrGroupsBase>() {
  return reinterpret_cast<TypeId>(10);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<SymbolicDescAttr>() {
  return reinterpret_cast<TypeId>(14);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<ShapeEnvAttr>() {
  return reinterpret_cast<TypeId>(15);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<af::AscTensorAttr>() {
  return reinterpret_cast<TypeId>(11);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<af::AscNodeAttr>() {
  return reinterpret_cast<TypeId>(12);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<af::AscGraphAttr>() {
  return reinterpret_cast<TypeId>(13);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<af::AutoFuseAttrs>() {
  return reinterpret_cast<TypeId>(16);
}

template<>
GE_FUNC_DEV_VISIBILITY GE_FUNC_HOST_VISIBILITY TypeId GetTypeId<af::AutoFuseGraphAttrs>() {
  return reinterpret_cast<TypeId>(17);
}

} // namespace ge
