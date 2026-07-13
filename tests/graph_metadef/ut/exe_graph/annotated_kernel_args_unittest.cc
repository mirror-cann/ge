/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "exe_graph/runtime/annotated_args_context.h"

#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "graph/utils/args_format_desc_utils.h"

#include <gtest/gtest.h>

namespace gert {
namespace {
template <typename T, typename = void>
struct HasGetStatus : std::false_type {};

template <typename T>
struct HasGetStatus<T, decltype(static_cast<void>(std::declval<const T &>().GetStatus()))> : std::true_type {};

template <typename T, typename = void>
struct HasGetImpl : std::false_type {};

template <typename T>
struct HasGetImpl<T, decltype(static_cast<void>(std::declval<const T &>().GetImpl()))> : std::true_type {};

static_assert(!HasGetStatus<AnnotatedKernelArgs>::value, "AnnotatedKernelArgs must not expose GetStatus");
static_assert(!HasGetImpl<AnnotatedKernelArgs>::value, "AnnotatedKernelArgs must not expose GetImpl");

TEST(AnnotatedKernelArgsUT, ExtractsArgsData) {
  const AnnotatedKernelArgs args(InputAddr{0U, reinterpret_cast<void *>(0x1000U)});
  std::vector<uint8_t> args_data;
  std::vector<ge::ArgDesc> arg_descs;

  EXPECT_EQ(args.ExtractArgsData(args_data, arg_descs), ge::GRAPH_SUCCESS);
  EXPECT_EQ(args_data.size(), sizeof(uint64_t));
  ASSERT_EQ(arg_descs.size(), 1U);
  EXPECT_EQ(arg_descs[0].addr_type, ge::AddrType::INPUT);
  EXPECT_EQ(arg_descs[0].ir_idx, 0);
}

TEST(AnnotatedKernelArgsUT, CopiesAndMovesRemainUsable) {
  AnnotatedKernelArgs source(InputAddr{0U, reinterpret_cast<void *>(0x1000U)},
                             OutputAddr{0U, reinterpret_cast<void *>(0x2000U)},
                             WorkspaceAddr{0U, reinterpret_cast<void *>(0x3000U)}, uint64_t{7U});
  EXPECT_EQ(source.AppendArg(uint64_t{8U}), ge::GRAPH_SUCCESS);

  AnnotatedKernelArgs copied(source);
  EXPECT_EQ(copied.AppendArg(InputAddr{1U, reinterpret_cast<void *>(0x4000U)}), ge::GRAPH_SUCCESS);

  AnnotatedKernelArgs moved(std::move(copied));
  EXPECT_EQ(moved.AppendArg(OutputAddr{1U, reinterpret_cast<void *>(0x5000U)}), ge::GRAPH_SUCCESS);
  EXPECT_NE(copied.AppendArg(uint64_t{9U}), ge::GRAPH_SUCCESS);
}

TEST(AnnotatedKernelArgsUT, RejectsInvalidIrIndexes) {
  constexpr uint32_t kInvalidIndex = static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) + 1U;
  AnnotatedKernelArgs invalid_input;
  AnnotatedKernelArgs invalid_output;
  AnnotatedKernelArgs invalid_workspace;

  EXPECT_NE(invalid_input.AppendArg(InputAddr{kInvalidIndex, reinterpret_cast<void *>(0x1000U)}), ge::GRAPH_SUCCESS);
  EXPECT_NE(invalid_output.AppendArg(OutputAddr{kInvalidIndex, reinterpret_cast<void *>(0x2000U)}), ge::GRAPH_SUCCESS);
  EXPECT_NE(invalid_workspace.AppendArg(WorkspaceAddr{kInvalidIndex, reinterpret_cast<void *>(0x3000U)}),
            ge::GRAPH_SUCCESS);
}

TEST(AnnotatedKernelArgsUT, AssignmentsPreserveUsabilityAndRejectMovedFromObject) {
  AnnotatedKernelArgs source(InputAddr{0U, reinterpret_cast<void *>(0x1000U)},
                             WorkspaceAddr{0U, reinterpret_cast<void *>(0x3000U)});
  AnnotatedKernelArgs copied;
  copied = source;
  EXPECT_EQ(copied.AppendArg(uint64_t{1U}), ge::GRAPH_SUCCESS);

  copied = copied;
  EXPECT_EQ(copied.AppendArg(uint64_t{2U}), ge::GRAPH_SUCCESS);

  AnnotatedKernelArgs move_target(OutputAddr{0U, reinterpret_cast<void *>(0x2000U)});
  move_target = std::move(copied);
  EXPECT_EQ(move_target.AppendArg(uint64_t{3U}), ge::GRAPH_SUCCESS);
  EXPECT_NE(copied.AppendArg(InputAddr{0U, reinterpret_cast<void *>(0x1000U)}), ge::GRAPH_SUCCESS);
  EXPECT_NE(copied.AppendArg(OutputAddr{0U, reinterpret_cast<void *>(0x2000U)}), ge::GRAPH_SUCCESS);
  EXPECT_NE(copied.AppendArg(WorkspaceAddr{0U, reinterpret_cast<void *>(0x3000U)}), ge::GRAPH_SUCCESS);
  EXPECT_NE(copied.AppendArg(uint64_t{1U}), ge::GRAPH_SUCCESS);

  AnnotatedKernelArgs copied_from_moved(copied);
  EXPECT_NE(copied_from_moved.AppendArg(InputAddr{0U, reinterpret_cast<void *>(0x1000U)}), ge::GRAPH_SUCCESS);
  AnnotatedKernelArgs assigned_from_moved(InputAddr{0U, reinterpret_cast<void *>(0x1000U)});
  assigned_from_moved = copied;
  EXPECT_NE(assigned_from_moved.AppendArg(InputAddr{0U, reinterpret_cast<void *>(0x1000U)}), ge::GRAPH_SUCCESS);

  AnnotatedKernelArgs self_move(InputAddr{0U, reinterpret_cast<void *>(0x1000U)});
  self_move = std::move(self_move);
  EXPECT_EQ(self_move.AppendArg(uint64_t{4U}), ge::GRAPH_SUCCESS);
}
}  // namespace
}  // namespace gert
