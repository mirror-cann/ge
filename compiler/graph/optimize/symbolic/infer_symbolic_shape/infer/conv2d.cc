/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "graph/compute_graph.h"
#include <string>
#include "common/checker.h"
#include "common/framework_types_internal.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"

namespace ge {
namespace {
// proto input index
constexpr size_t kXIdxConv2d = 0U;
constexpr size_t kWIdxConv2d = 1U;
// proto output index
constexpr size_t kYIdxConv2d = 0U;
// proto attribute index
constexpr size_t kStridesIdxConv2d = 0U;
constexpr size_t kPadsIdxConv2d = 1U;
constexpr size_t kDilationsIdxConv2d = 2U;
constexpr size_t kGroupsIdxConv2d = 3U;
// attr 索引 6：Conv2D 对应 "padding"，Conv2DV2 对应 "pad_mode"。
// 二者本质都是 pad 模式字符串，统一走同一套分支处理。
constexpr size_t kPaddingIdxConv2d = 6U;
// NCHW shape
constexpr int32_t kNCHWdimNIdxConv2d = 0;
constexpr int32_t kNCHWdimCIdxConv2d = 1;
constexpr int32_t kNCHWdimHIdxConv2d = 2;
constexpr int32_t kNCHWdimWIdxConv2d = 3;
// NHWC shape
constexpr int32_t kNHWCdimNIdxConv2d = 0;
constexpr int32_t kNHWCdimCIdxConv2d = 3;
constexpr int32_t kNHWCdimHIdxConv2d = 1;
constexpr int32_t kNHWCdimWIdxConv2d = 2;
// HWCN shape
constexpr int32_t kHWCNdimNIdxConv2d = 3;
constexpr int32_t kHWCNdimCIdxConv2d = 2;
constexpr int32_t kHWCNdimHIdxConv2d = 0;
constexpr int32_t kHWCNdimWIdxConv2d = 1;
// PAD IDX
constexpr int32_t kPadTopIdxConv2d = 0;
constexpr int32_t kPadBottomIdxConv2d = 1;
constexpr int32_t kPadLeftIdxConv2d = 2;
constexpr int32_t kPadRightIdxConv2d = 3;

// support information
constexpr size_t kSupportedDimNumConv2d = 4U;
constexpr size_t kPadLimitSizeConv2d = 4U;
constexpr size_t kStrideLimitSizeConv2d = 4U;
constexpr size_t kDilationLimitSizeConv2d = 4U;

struct Conv2DInputShapes {
  Expression in{kSymbolZero};
  Expression ic{kSymbolZero};
  Expression ih{kSymbolZero};
  Expression iw{kSymbolZero};
};

struct Conv2DOutputShape {
  Expression on{kSymbolZero};
  Expression oc{kSymbolZero};
  Expression oh{kSymbolZero};
  Expression ow{kSymbolZero};
  bool is_zero_tensor = false;
};

struct Conv2DAttrs {
  Expression strh{kSymbolZero};
  Expression strw{kSymbolZero};
  Expression dilh{kSymbolZero};
  Expression dilw{kSymbolZero};
  Expression padt{kSymbolZero};
  Expression padb{kSymbolZero};
  Expression padl{kSymbolZero};
  Expression padr{kSymbolZero};
};

ge::graphStatus GetConv2DXShapeDim(const gert::InferSymbolShapeContext *context, Conv2DInputShapes &x_shapes) {
  // Get x format
  GE_CHECK_NOTNULL(context);
  const gert::CompileTimeTensorDesc *x_desc_ptr = context->GetInputDesc(kXIdxConv2d);
  GE_ASSERT_NOTNULL(x_desc_ptr);
  const ge::Format x_format = x_desc_ptr->GetOriginFormat();
  // Get x shape
  const auto x_shape = context->GetInputSymbolShape(kXIdxConv2d);
  GE_UNSUPPORTED_IF_NULL(x_shape);
  GE_ASSERT_TRUE(x_shape->GetDimNum() == kSupportedDimNumConv2d, "input x_shape dim num should be %lu but is %lu.",
                 kSupportedDimNumConv2d, x_shape->GetDimNum());
  // Set shapes
  if (x_format == ge::Format::FORMAT_NCHW) {
    x_shapes.in = x_shape->GetDim(kNCHWdimNIdxConv2d);
    x_shapes.ic = x_shape->GetDim(kNCHWdimCIdxConv2d);
    x_shapes.ih = x_shape->GetDim(kNCHWdimHIdxConv2d);
    x_shapes.iw = x_shape->GetDim(kNCHWdimWIdxConv2d);
  } else if (x_format == ge::Format::FORMAT_NHWC) {
    x_shapes.in = x_shape->GetDim(kNHWCdimNIdxConv2d);
    x_shapes.ic = x_shape->GetDim(kNHWCdimCIdxConv2d);
    x_shapes.ih = x_shape->GetDim(kNHWCdimHIdxConv2d);
    x_shapes.iw = x_shape->GetDim(kNHWCdimWIdxConv2d);
  } else {
    GELOGE(PARAM_INVALID, "input x format should be NCHW or NHWC");
    return ge::PARAM_INVALID;
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus GetConv2DWShapeDim(const gert::InferSymbolShapeContext *context, Conv2DInputShapes &w_shapes) {
  // Get w format
  GE_CHECK_NOTNULL(context);
  const gert::CompileTimeTensorDesc *w_desc_ptr = context->GetInputDesc(kWIdxConv2d);
  GE_ASSERT_NOTNULL(w_desc_ptr);
  const ge::Format w_format = w_desc_ptr->GetOriginFormat();
  // Get w shape
  const auto w_shape = context->GetInputSymbolShape(kWIdxConv2d);
  GE_UNSUPPORTED_IF_NULL(w_shape);
  GE_ASSERT_TRUE(w_shape->GetDimNum() == kSupportedDimNumConv2d, "Not support input x_shape dim num %lu.",
                 w_shape->GetDimNum());
  // Set shapes
  if (w_format == ge::Format::FORMAT_NCHW) {
    w_shapes.in = w_shape->GetDim(kNCHWdimNIdxConv2d);
    w_shapes.ic = w_shape->GetDim(kNCHWdimCIdxConv2d);
    w_shapes.ih = w_shape->GetDim(kNCHWdimHIdxConv2d);
    w_shapes.iw = w_shape->GetDim(kNCHWdimWIdxConv2d);
  } else if (w_format == ge::Format::FORMAT_NHWC) {
    w_shapes.in = w_shape->GetDim(kNHWCdimNIdxConv2d);
    w_shapes.ic = w_shape->GetDim(kNHWCdimCIdxConv2d);
    w_shapes.ih = w_shape->GetDim(kNHWCdimHIdxConv2d);
    w_shapes.iw = w_shape->GetDim(kNHWCdimWIdxConv2d);
  } else if (w_format == ge::Format::FORMAT_HWCN) {
    w_shapes.in = w_shape->GetDim(kHWCNdimNIdxConv2d);
    w_shapes.ic = w_shape->GetDim(kHWCNdimCIdxConv2d);
    w_shapes.ih = w_shape->GetDim(kHWCNdimHIdxConv2d);
    w_shapes.iw = w_shape->GetDim(kHWCNdimWIdxConv2d);
  } else {
    GELOGE(PARAM_INVALID, "input filter format should be NCHW, NHWC or HWCN");
    return ge::PARAM_INVALID;
  }
  ASSERT_SYMBOL_GT(w_shapes.ih, kSymbolZero);
  ASSERT_SYMBOL_GT(w_shapes.iw, kSymbolZero);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus CheckGroupsAndOutChannel(const Expression& out_channel, const Expression& groups) {
  // 分组卷积的拓扑约束：
  // 输出通道会按 groups 划分到各组，要求每组通道数一致，
  // 因此必须满足 out_channel % groups == 0。
  if (EXPECT_SYMBOL_NE(groups, kSymbolZero)) {
    const auto mod_result = sym::Mod(out_channel, groups);
    ASSERT_SYMBOL_EQ(mod_result, kSymbolZero);
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus CheckConv2DGroups(const gert::InferSymbolShapeContext *context, const Conv2DInputShapes &x_shapes,
                                  const Conv2DInputShapes &w_shapes) {
  GE_CHECK_NOTNULL(context);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto group_ptr = attrs->GetAttrPointer<int64_t>(kGroupsIdxConv2d);
  GE_ASSERT_NOTNULL(group_ptr);
  const int64_t groups = *group_ptr;
  // groups 是显式整型 attr，<=0 一律非法。
  // （host infer 在未知形状场景可能跳过该检查；符号推导路径基于符号表达式，
  //  这里直接校验 attr 值。）
  if (groups <= 0) {
    GELOGE(PARAM_INVALID, "groups %ld is invalid.", groups);
    return ge::PARAM_INVALID;
  }

  Expression ex_groups = Symbol(groups);
  const auto *node_type = context->GetNodeType();
  const bool is_conv2dv2 = (node_type != nullptr) && (std::string(node_type) == "Conv2DV2");
  if (!is_conv2dv2) {
    // Conv2D：保持历史逻辑，避免 UT/现网行为回归。
    // 当 groups == 1 且 kc != 0，尝试按 ic/kc 推导有效 groups。
    if (groups == 1 && EXPECT_SYMBOL_NE(w_shapes.ic, kSymbolZero)) {
      const auto mod_result = sym::Mod(x_shapes.ic, w_shapes.ic);
      if (EXPECT_SYMBOL_EQ(mod_result, kSymbolZero)) {
        ex_groups = x_shapes.ic / w_shapes.ic;
      } else {
        GELOGE(PARAM_INVALID, "in_channels(>0) should be divisible by kernel_channels when groups = 1.");
        return ge::PARAM_INVALID;
      }
    }
  } else {
    // Conv2DV2：对齐 op_host 语义。
    // groups == 1 且 ic/kc 均非 0 时，允许隐式改写 groups = ic/kc。
    if (groups == 1 && EXPECT_SYMBOL_NE(x_shapes.ic, kSymbolZero) && EXPECT_SYMBOL_NE(w_shapes.ic, kSymbolZero)) {
      const auto mod_result = sym::Mod(x_shapes.ic, w_shapes.ic);
      if (EXPECT_SYMBOL_EQ(mod_result, kSymbolZero)) {
        ex_groups = x_shapes.ic / w_shapes.ic;
      } else {
        GELOGE(PARAM_INVALID, "in_channels(>0) should be divisible by kernel_channels when groups = 1.");
        return ge::PARAM_INVALID;
      }
    }
    // Conv2DV2 额外强约束：ic 必须等于 kc * effective_groups。
    ASSERT_SYMBOL_EQ(x_shapes.ic, w_shapes.ic * ex_groups);
  }

  // Conv2D/Conv2DV2 公共约束：输出通道数必须能被有效 groups 整除。
  if (CheckGroupsAndOutChannel(w_shapes.in, ex_groups) != ge::GRAPH_SUCCESS) {
    GELOGE(PARAM_INVALID, "out_channels should be divisible by groups.");
    return ge::PARAM_INVALID;
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus GetConv2DStrides(const gert::InferSymbolShapeContext *context, Conv2DAttrs &conv2d_attrs) {
  GE_CHECK_NOTNULL(context);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto *strides_ptr = attrs->GetAttrPointer<gert::ContinuousVector>(kStridesIdxConv2d);
  GE_ASSERT_NOTNULL(strides_ptr);
  if (strides_ptr->GetSize() != kStrideLimitSizeConv2d) {
    GELOGE(PARAM_INVALID, "strides list should be 4D.");
    return ge::PARAM_INVALID;
  }
  const auto strides_array = static_cast<const int64_t *>(strides_ptr->GetData());
  GE_ASSERT_NOTNULL(strides_array);
  GE_ASSERT_NOTNULL(context->GetInputDesc(kXIdxConv2d));
  const ge::Format x_format = context->GetInputDesc(kXIdxConv2d)->GetOriginFormat();
  int64_t strideh = 0;
  int64_t stridew = 0;
  if (x_format == ge::Format::FORMAT_NCHW) {
    strideh = strides_array[kNCHWdimHIdxConv2d];
    stridew = strides_array[kNCHWdimWIdxConv2d];
  } else if (x_format == ge::Format::FORMAT_NHWC) {
    strideh = strides_array[kNHWCdimHIdxConv2d];
    stridew = strides_array[kNHWCdimWIdxConv2d];
  }
  if (strideh <= 0 || stridew <= 0) {
    GELOGE(PARAM_INVALID, "Conv2D attr strides should be positive");
    return ge::PARAM_INVALID;
  }
  conv2d_attrs.strh = Symbol(strideh);
  conv2d_attrs.strw = Symbol(stridew);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus GetConv2DDilations(const gert::InferSymbolShapeContext *context, Conv2DAttrs &conv2d_attrs) {
  const gert::RuntimeAttrs *attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto *dilations_ptr = attrs->GetAttrPointer<gert::ContinuousVector>(kDilationsIdxConv2d);
  GE_ASSERT_NOTNULL(dilations_ptr);
  if (dilations_ptr->GetSize() != kDilationLimitSizeConv2d) {
    GELOGW("dilations list should be 4D.");
    return ge::PARAM_INVALID;
  }

  const auto dilations_array = static_cast<const int64_t *>(dilations_ptr->GetData());
  GE_ASSERT_NOTNULL(dilations_array);
  GE_ASSERT_NOTNULL(context->GetInputDesc(kXIdxConv2d));
  const ge::Format x_format = context->GetInputDesc(kXIdxConv2d)->GetOriginFormat();
  int64_t dilath = 0;
  int64_t dilatw = 0;
  if (x_format == ge::Format::FORMAT_NCHW) {
    dilath = dilations_array[kNCHWdimHIdxConv2d];
    dilatw = dilations_array[kNCHWdimWIdxConv2d];
  } else if (x_format == ge::Format::FORMAT_NHWC) {
    dilath = dilations_array[kNHWCdimHIdxConv2d];
    dilatw = dilations_array[kNHWCdimWIdxConv2d];
  }
  if (dilath <= 0 || dilatw <= 0) {
    GELOGW("Conv2D attr dilations should be positive");
    return ge::PARAM_INVALID;
  }
  conv2d_attrs.dilh = Symbol(dilath);
  conv2d_attrs.dilw = Symbol(dilatw);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus CutPads(const Conv2DInputShapes &x_shapes, const Conv2DInputShapes &w_shapes, Conv2DAttrs &attrs) {
  // 当输出尺寸恰好为 1 时，存在“右/下侧 pad 过量但不改变输出”的场景。
  // 这里把可裁掉的余量从 bottom/right 中扣除，目的：
  // 1) 保持与 host 端推导一致；
  // 2) 让符号表达式更简洁，减少后续不必要约束。
  const Expression ho =
      (x_shapes.ih + attrs.padt + attrs.padb - (w_shapes.ih - kSymbolOne) * attrs.dilh - kSymbolOne) / attrs.strh +
      kSymbolOne;
  Expression hr = (x_shapes.ih + attrs.padt + attrs.padb - (w_shapes.ih - kSymbolOne) * attrs.dilh - kSymbolOne);
  hr = sym::Mod(hr, attrs.strh);
  if (EXPECT_SYMBOL_EQ(ho, kSymbolOne) && EXPECT_SYMBOL_LE(hr, attrs.padb)) {
    attrs.padb = attrs.padb - hr;
  }
  const Expression wo =
      (x_shapes.iw + attrs.padl + attrs.padr - (w_shapes.iw - kSymbolOne) * attrs.dilw - kSymbolOne) / attrs.strw +
      kSymbolOne;
  Expression wr = (x_shapes.iw + attrs.padl + attrs.padr - (w_shapes.iw - kSymbolOne) * attrs.dilw - kSymbolOne);
  wr = sym::Mod(wr, attrs.strw);
  if (EXPECT_SYMBOL_EQ(wo, kSymbolOne) && EXPECT_SYMBOL_LE(wr, attrs.padr)) {
    attrs.padr = attrs.padr - wr;
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus CheckPositivePads(const Conv2DAttrs &conv2d_attrs) {
  ASSERT_SYMBOL_GE(conv2d_attrs.padt, kSymbolZero);
  ASSERT_SYMBOL_GE(conv2d_attrs.padb, kSymbolZero);
  ASSERT_SYMBOL_GE(conv2d_attrs.padl, kSymbolZero);
  ASSERT_SYMBOL_GE(conv2d_attrs.padr, kSymbolZero);
  return ge::GRAPH_SUCCESS;
}

void SetAllPadsZero(Conv2DAttrs &conv2d_attrs) {
  conv2d_attrs.padt = kSymbolZero;
  conv2d_attrs.padb = kSymbolZero;
  conv2d_attrs.padl = kSymbolZero;
  conv2d_attrs.padr = kSymbolZero;
}

Expression InferSameModePadNeeded(const Expression &input, const Expression &kernel, const Expression &stride,
                                  const Expression &dilation) {
  const auto tails = sym::Mod(input, stride);
  const auto dk = dilation * (kernel - kSymbolOne) + kSymbolOne;
  if (EXPECT_SYMBOL_GT(tails, kSymbolZero)) {
    return dk - tails;
  }
  return dk - stride;
}

void SplitPadByMode(const Expression &pad, const bool same_lower, Expression &pad_begin, Expression &pad_end) {
  if (!EXPECT_SYMBOL_GT(pad, kSymbolZero)) {
    pad_begin = kSymbolZero;
    pad_end = kSymbolZero;
    return;
  }
  const auto half = sym::Floor(pad / kSymbolTwo);
  const auto rem = sym::Mod(pad, kSymbolTwo);
  if (same_lower) {
    pad_begin = half + rem;
    pad_end = half;
    return;
  }
  pad_begin = half;
  pad_end = half + rem;
}

ge::graphStatus InferConv2DSameModePads(const Conv2DInputShapes &x_shapes, const Conv2DInputShapes &w_shapes,
                                        const std::string &padding_mode, Conv2DAttrs &conv2d_attrs) {
  const auto pad_h = InferSameModePadNeeded(x_shapes.ih, w_shapes.ih, conv2d_attrs.strh, conv2d_attrs.dilh);
  const auto pad_w = InferSameModePadNeeded(x_shapes.iw, w_shapes.iw, conv2d_attrs.strw, conv2d_attrs.dilw);
  const bool same_lower = (padding_mode == "SAME_LOWER");
  SplitPadByMode(pad_h, same_lower, conv2d_attrs.padt, conv2d_attrs.padb);
  SplitPadByMode(pad_w, same_lower, conv2d_attrs.padl, conv2d_attrs.padr);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus InferConv2DPadsWithLegacySame(const Conv2DInputShapes &x_shapes, const Conv2DInputShapes &w_shapes,
                                              Conv2DAttrs &conv2d_attrs) {
  const auto tails_h = sym::Mod(x_shapes.ih, conv2d_attrs.strh);
  const auto tails_w = sym::Mod(x_shapes.iw, conv2d_attrs.strw);
  const auto dk_h = conv2d_attrs.dilh * (w_shapes.ih - kSymbolOne) + kSymbolOne;
  const auto dk_w = conv2d_attrs.dilw * (w_shapes.iw - kSymbolOne) + kSymbolOne;
  auto pad_h = kSymbolZero;
  auto pad_w = kSymbolZero;
  if (EXPECT_SYMBOL_GT(tails_h, kSymbolZero)) {
    pad_h = dk_h - tails_h;
    pad_w = dk_w - tails_w;
  } else {
    pad_h = dk_h - conv2d_attrs.strh;
    pad_w = dk_w - conv2d_attrs.strw;
  }
  SplitPadByMode(pad_h, false, conv2d_attrs.padt, conv2d_attrs.padb);
  SplitPadByMode(pad_w, false, conv2d_attrs.padl, conv2d_attrs.padr);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus InferConv2DPadsWithPadding(const size_t pad_size, const Conv2DInputShapes &x_shapes,
                                           const Conv2DInputShapes &w_shapes, const std::string &padding_mode,
                                           const bool is_conv2dv2, Conv2DAttrs &conv2d_attrs) {
  if (padding_mode.empty()) {
    return ge::GRAPH_SUCCESS;  // no padding on node, return
  }
  if (!is_conv2dv2) {
    if (padding_mode == "EXPLICIT") {
      GE_ASSERT_TRUE(pad_size == kPadLimitSizeConv2d, "pads list should be 4D. actual is: %zu.", pad_size);
      return ge::GRAPH_SUCCESS;
    }
    if (padding_mode == "SAME") {
      return InferConv2DPadsWithLegacySame(x_shapes, w_shapes, conv2d_attrs);
    }
    if (padding_mode == "VALID") {
      SetAllPadsZero(conv2d_attrs);
      return ge::GRAPH_SUCCESS;
    }
    GELOGE(PARAM_INVALID, "padding should be EXPLICIT or SAME or VALID, but the actual is: %s.", padding_mode.c_str());
    return ge::PARAM_INVALID;
  }
  if ((padding_mode == "EXPLICIT") || (padding_mode == "SPECIFIC")) {
    GE_ASSERT_TRUE(pad_size == kPadLimitSizeConv2d, "pads list should be 4D. actual is: %zu.", pad_size);
    return ge::GRAPH_SUCCESS;
  }
  if ((padding_mode == "SAME") || (padding_mode == "SAME_UPPER") || (padding_mode == "SAME_LOWER")) {
    return InferConv2DSameModePads(x_shapes, w_shapes, padding_mode, conv2d_attrs);
  }
  if (padding_mode == "VALID") {
    SetAllPadsZero(conv2d_attrs);
    return ge::GRAPH_SUCCESS;
  }
  GELOGE(PARAM_INVALID, "padding should be EXPLICIT/SPECIFIC or SAME/SAME_UPPER/SAME_LOWER or VALID, but the actual is: %s.",
         padding_mode.c_str());
  return ge::PARAM_INVALID;
}

ge::graphStatus GetConv2DPads(const gert::InferSymbolShapeContext *context, const Conv2DInputShapes &x_shapes,
                              const Conv2DInputShapes &w_shapes, Conv2DAttrs &conv2d_attrs) {
  GE_ASSERT_NOTNULL(context);
  const gert::RuntimeAttrs *attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto *pads_ptr = attrs->GetAttrPointer<gert::ContinuousVector>(kPadsIdxConv2d);
  GE_ASSERT_NOTNULL(pads_ptr);
  const size_t pads_size = pads_ptr->GetSize();
  if (pads_size == kPadLimitSizeConv2d) {
    const auto *pads_array = static_cast<const int64_t *>(pads_ptr->GetData());
    GE_ASSERT_NOTNULL(pads_array);
    conv2d_attrs.padt = Symbol(static_cast<int64_t>(pads_array[kPadTopIdxConv2d]));
    conv2d_attrs.padb = Symbol(static_cast<int64_t>(pads_array[kPadBottomIdxConv2d]));
    conv2d_attrs.padl = Symbol(static_cast<int64_t>(pads_array[kPadLeftIdxConv2d]));
    conv2d_attrs.padr = Symbol(static_cast<int64_t>(pads_array[kPadRightIdxConv2d]));
  }
  // Infer pads if "padding/pad_mode" is defined.
  if (attrs->GetAttrNum() > kPaddingIdxConv2d) {
    const char *padding_ptr = attrs->GetAttrPointer<char>(kPaddingIdxConv2d);
    GE_ASSERT_NOTNULL(padding_ptr);
    const auto *node_type = context->GetNodeType();
    const bool is_conv2dv2 = (node_type != nullptr) && (std::string(node_type) == "Conv2DV2");
    GE_ASSERT_GRAPH_SUCCESS(InferConv2DPadsWithPadding(pads_size, x_shapes, w_shapes, padding_ptr, is_conv2dv2,
                                                       conv2d_attrs));
  }
  // >>> start: cut off right/bottom pad when output size is 1
  CutPads(x_shapes, w_shapes, conv2d_attrs);
  // <<< end: cut off right/bottom pad when output size is 1
  return CheckPositivePads(conv2d_attrs);
}

ge::graphStatus CheckConv2DInputWithPad(const Expression &ih_pad, const Expression &iw_pad) {
  // 几何合法性：补 pad 后的有效输入区域，必须覆盖膨胀卷积核窗口。
  // 若 ih_pad/iw_pad < 0，表示窗口大于有效输入，卷积无定义。
  ASSERT_SYMBOL_GE(ih_pad, kSymbolZero);
  ASSERT_SYMBOL_GE(iw_pad, kSymbolZero);
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus SetConv2DYShape(gert::InferSymbolShapeContext *context, const Expression& in, const Expression& kn,
                                const Expression& oh, const Expression& ow) {
  gert::SymbolShape *y_shape = context->GetOutputSymbolShape(kYIdxConv2d);
  GE_CHECK_NOTNULL(y_shape);
  y_shape->Clear();
  const auto x_desc_ptr = context->GetInputDesc(kXIdxConv2d);
  GE_ASSERT_NOTNULL(x_desc_ptr);
  const auto x_format = x_desc_ptr->GetOriginFormat();
  if (x_format == ge::Format::FORMAT_NCHW) {
    y_shape->AppendDim(in);
    y_shape->AppendDim(kn);
    y_shape->AppendDim(oh);
    y_shape->AppendDim(ow);
  } else if (x_format == ge::Format::FORMAT_NHWC) {
    y_shape->AppendDim(in);
    y_shape->AppendDim(oh);
    y_shape->AppendDim(ow);
    y_shape->AppendDim(kn);
  }
  return ge::GRAPH_SUCCESS;
}

void InferOutputIsZeroTensor(const Conv2DInputShapes &x_shapes, const Conv2DInputShapes &w_shapes,
                             Conv2DOutputShape &y_shape) {
  // 只要输入特征图或卷积核的关键维度出现 0，输出进入 zero tensor 路径。
  // 这不是错误场景，而是框架允许的退化形态。
  if (EXPECT_SYMBOL_EQ(x_shapes.in, kSymbolZero) || EXPECT_SYMBOL_EQ(x_shapes.ic, kSymbolZero) ||
      EXPECT_SYMBOL_EQ(x_shapes.ih, kSymbolZero) || EXPECT_SYMBOL_EQ(x_shapes.iw, kSymbolZero) ||
      EXPECT_SYMBOL_EQ(w_shapes.in, kSymbolZero) || EXPECT_SYMBOL_EQ(w_shapes.ic, kSymbolZero)) {
    // input is zero tensor
    y_shape.is_zero_tensor = true;
  }
}

ge::graphStatus CheckOutputZeroTensor(const Conv2DOutputShape &y_shape) {
  // zero tensor 一致性校验：
  // 若标记为 zero tensor，则输出维度不能全部是非 0（否则与标记矛盾）。
  if (!y_shape.is_zero_tensor) {
    return ge::GRAPH_SUCCESS;
  }
  ASSERT_SYMBOL_GE(y_shape.on, kSymbolZero);
  ASSERT_SYMBOL_GE(y_shape.oc, kSymbolZero);
  ASSERT_SYMBOL_GE(y_shape.oh, kSymbolZero);
  ASSERT_SYMBOL_GE(y_shape.ow, kSymbolZero);
  if (EXPECT_SYMBOL_NE(y_shape.on, kSymbolZero) && EXPECT_SYMBOL_NE(y_shape.oc, kSymbolZero) &&
      EXPECT_SYMBOL_NE(y_shape.oh, kSymbolZero) && EXPECT_SYMBOL_NE(y_shape.ow, kSymbolZero)) {
    GELOGE(PARAM_INVALID, "output shape should be zero tensor but dims are not zero");
    return ge::PARAM_INVALID;
  }
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus CheckFcKcEqualInZeroTensor(const Conv2DInputShapes &x_shapes, const Conv2DInputShapes &w_shapes) {
  // 当 ic/kc 存在 0 时，额外保持 x.ic 与 w.ic 的一致性约束，
  // 防止 zero tensor 场景下出现“通道语义不一致但被掩盖”的情况。
  if (EXPECT_SYMBOL_NE(x_shapes.ic, kSymbolZero) && EXPECT_SYMBOL_NE(w_shapes.ic, kSymbolZero)) {
    return ge::GRAPH_SUCCESS;
  }
  ASSERT_SYMBOL_EQ(x_shapes.ic, w_shapes.ic);
  return ge::GRAPH_SUCCESS;
}

/**
 * Conv2D/Conv2DV2 算子的符号化shape推导
 * 【算子功能】计算二维卷积操作，支持多种输入格式和padding模式
 *            输入：x(输入特征图)、filter(卷积核)两个张量
 *            输出：y(卷积结果)张量
 * 【算子约束】
 *      1. 支持的输入格式：NCHW、NHWC；filter支持NCHW、NHWC、HWCN
 *      2. strides属性指定卷积步长，必须为正整数
 *      3. dilations属性指定膨胀率，必须为正整数
 *      4. groups属性指定分组卷积数，必须大于0
 *      5. padding/pad_mode属性支持EXPLICIT/SPECIFIC、SAME/SAME_UPPER/SAME_LOWER、VALID
 * 【推导逻辑】
 *      1. 解析输入张量x和卷积核filter的逻辑维度(N,C,H,W)
 *      2. 处理groups语义，校验分组约束(输出通道必须能被groups整除)
 *      3. 解析stride、dilation、pad属性，处理SAME模式的自动padding计算
 *      4. 应用标准卷积公式推导输出尺寸：
 *         out = floor((in + pad_before + pad_after - dilation * (kernel - 1) - 1) / stride) + 1
 *      5. zero tensor分支做一致性校验，处理退化场景
 *      6. 按输入layout(NCHW/NHWC)回写输出shape
 *      举例：输入x_shape=(2,3,28,28)，filter_shape=(16,3,3,3)，strides=[1,1,1,1]，padding=SAME，
 *            则y_shape=(2,16,28,28)
 */
graphStatus InferShape4Conv2D(gert::InferSymbolShapeContext *context) {
  // 主流程（先归一化，再公式化）：
  // 1) 解析 x/filter 逻辑维度；
  // 2) 处理 groups 语义并落约束；
  // 3) 解析 stride/dilation/pad；
  // 4) 用统一卷积公式构建 oh/ow；
  // 5) zero tensor 分支做一致性校验；
  // 6) 按输入 layout 回写输出 shape。
  auto x_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(x_shape);
  auto filter_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(filter_shape);
  // 可选输入 bias/offset_w 不参与 shape 公式，
  // 符号推导只依赖 x/filter 的 shape 与 attrs。
  auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);

  Conv2DInputShapes x_shapes;
  Conv2DInputShapes w_shapes;
  GE_ASSERT_GRAPH_SUCCESS(GetConv2DXShapeDim(context, x_shapes));
  GE_ASSERT_GRAPH_SUCCESS(GetConv2DWShapeDim(context, w_shapes));
  GE_ASSERT_GRAPH_SUCCESS(CheckConv2DGroups(context, x_shapes, w_shapes));
  Conv2DAttrs conv2d_attrs;
  GE_ASSERT_GRAPH_SUCCESS(GetConv2DStrides(context, conv2d_attrs));
  GE_ASSERT_GRAPH_SUCCESS(GetConv2DDilations(context, conv2d_attrs));
  GE_ASSERT_GRAPH_SUCCESS(GetConv2DPads(context, x_shapes, w_shapes, conv2d_attrs));

  // 标准 Conv2D 公式：
  // out = floor((in + pad_before + pad_after - dilation * (kernel - 1) - 1) / stride) + 1
  Expression ih_pad =
      x_shapes.ih + conv2d_attrs.padt + conv2d_attrs.padb - conv2d_attrs.dilh * (w_shapes.ih - kSymbolOne) - kSymbolOne;
  Expression iw_pad =
      x_shapes.iw + conv2d_attrs.padl + conv2d_attrs.padr - conv2d_attrs.dilw * (w_shapes.iw - kSymbolOne) - kSymbolOne;
  Expression oh = sym::Floor(ih_pad / conv2d_attrs.strh) + kSymbolOne;
  Expression ow = sym::Floor(iw_pad / conv2d_attrs.strw) + kSymbolOne;
  Conv2DOutputShape y_shape;
  y_shape.on = x_shapes.in;
  // 若已知 x.ic 为 0，则输出通道强制为 0；否则取 filter 的输出通道数。
  y_shape.oc = EXPECT_SYMBOL_EQ(x_shapes.ic, kSymbolZero) ? kSymbolZero : w_shapes.in;
  y_shape.oh = oh;
  y_shape.ow = ow;
  InferOutputIsZeroTensor(x_shapes, w_shapes, y_shape);
  GE_ASSERT_GRAPH_SUCCESS(CheckFcKcEqualInZeroTensor(x_shapes, w_shapes));

  // zero tensor 输入是框架允许的合法路径：
  // 跳过 ih/iw 的 padded-size 合法性检查，转由 zero-tensor 规则校验。
  if (!y_shape.is_zero_tensor) {
    GE_ASSERT_GRAPH_SUCCESS(CheckConv2DInputWithPad(ih_pad, iw_pad));
  }
  GE_ASSERT_GRAPH_SUCCESS(CheckOutputZeroTensor(y_shape));
  GE_ASSERT_GRAPH_SUCCESS(SetConv2DYShape(context, y_shape.on, y_shape.oc, y_shape.oh, y_shape.ow));
  return ge::GRAPH_SUCCESS;
}
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Conv2D).InferSymbolShape(InferShape4Conv2D);
// Conv2DV2 与 Conv2D 复用同一套几何公式，
// 语义差异在上面的 attr/groups 分支中处理。
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Conv2DV2).InferSymbolShape(InferShape4Conv2D);
}  // namespace
}  // namespace ge
