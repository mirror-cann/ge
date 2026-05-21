/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// This header is force-included at the top of every aihacb_autofusion_stub compilation unit
// via -include to resolve conflicts between cann-9.x and local ge metadef headers.
//
// Root cause: cann's ascendc_ir_def.h (in ascendc_ir_core/) includes, via same-directory
// relative paths:
//   attr_serializer_registry.h -> attr_serializer.h (cann version, guard: AF_METADEF_CXX_ATTR_SERIALIZER_H)
// The cann attr_serializer.h under AUTOFUSE_USE_GE_METADEF does: using GeIrAttrSerializer = ge::GeIrAttrSerializer;
// but ge::GeIrAttrSerializer is only defined in the local attr_serializer.h.
// Same-directory relative includes bypass -I path ordering, so BEFORE paths cannot intercept them.
//
// Fix: pre-include the local versions of these two headers first.
// Since both use the same include guards as the cann versions
// (METADEF_CXX_ATTR_SERIALIZER_H / METADEF_CXX_ATTR_SERIALIZER_REGISTRY_H),
// the cann versions will be silently skipped when encountered via relative include.

#include "graph/attr_store.h"
#include "graph/serialization/attr_serializer.h"
#include "graph/serialization/attr_serializer_registry.h"

#define AF_EXECUTE_GRAPH_ATTR_STORE_H
#define AF_METADEF_CXX_ATTR_SERIALIZER_H
