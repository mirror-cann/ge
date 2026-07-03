/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// Bridge API that lets GE add nodes into AscGraph without instantiating af::Operator or
// af::ascir_op types in GE translation units.  All af-internal construction happens inside
// libaihac_ir.so, which is compiled without AUTOFUSE_USE_GE_METADEF.
//
// GE callers use either an existing OpDescPtr or a narrow helper for a specific
// ascir_op type. For other ascir_op types used in MakeRawNode<T>, GE calls
// AscGraphAddAscirNodeByType() and libaihac_ir.so dispatches by T::Type string.

#ifndef METADEF_CXX_ASC_GRAPH_GE_BRIDGE_H_
#define METADEF_CXX_ASC_GRAPH_GE_BRIDGE_H_

#include <cstdint>
#include <string>
#include <vector>
#include "graph/ascendc_ir/ascendc_ir_core/ascendc_ir.h"

namespace af {

// Pattern A: add a node from an existing OpDescPtr.
// The OpDesc must already have IR inputs/outputs declared (AppendIrInput / AppendIrOutput).
// AscNodeAttr is created automatically inside AscNode's constructor.
AscNodePtr AscGraphAddNodeFromOpDesc(AscGraph &asc_graph, OpDescPtr op_desc);

// Pattern B helpers for specific ascir_op types that GE constructs directly.

// Adds a Split node with the requested output count.
AscNodePtr AscGraphAddSplitNode(AscGraph &asc_graph, const char *name, uint32_t out_num);

// Generic helper for ascir_op types used via MakeRawNode<T> in asc_overrides.h.
// Constructs the named ascir_op inside libaihac_ir.so and adds it to asc_graph.
// Positive dynamic input and output counts request dynamic input/output registration.
AscNodePtr AscGraphAddAscirNodeByType(AscGraph &asc_graph, const char *op_type, const char *name,
                                      size_t dynamic_input_num, size_t dynamic_output_num);

AxisId AscGraphCreateAxisBySerializedExpr(AscGraph &asc_graph, const char *name, const char *serialized_expr);

bool IsScalarInputBySerializedExprs(const std::vector<std::string> &serialized_exprs);

// GE-side stub helpers: avoid af::Operator symbol-mangling mismatch caused by
// AUTOFUSE_USE_GE_METADEF (which makes af::Operator an alias for ge::Operator in GE TUs,
// producing different mangled names than libaihac_ir.so expects).
//
// Both af::OpDesc and ge::OpDesc have identical memory layouts (same source, different
// namespace).  GE passes the raw pointer so no shared_ptr type conversion is needed.
//
// Used by ge autofuse_backend_stub.cpp when it already owns a GE OpDesc.
AscTensorAttr *AscTensorAttrGetOrCreateForOpOutput(void *op_desc_raw, uint32_t index);
AscNodeAttr *AscNodeAttrGetOrCreateForOp(void *op_desc_raw);

// Same as AscGraphAddNodeFromOpDesc but accepts a raw pointer to avoid shared_ptr
// type-conversion mismatch when called from GE TUs compiled with AUTOFUSE_USE_GE_METADEF
// (where af::OpDescPtr == ge::OpDescPtr but their C++ mangled names differ).
// Used by ge autofuse_backend_stub.cpp when adding a node from an existing GE OpDesc.
AscNodePtr AscGraphAddNodeFromOpDescRaw(AscGraph &asc_graph, void *op_desc_raw);

}  // namespace af

#endif  // METADEF_CXX_ASC_GRAPH_GE_BRIDGE_H_
