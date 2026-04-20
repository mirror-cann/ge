/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_CUSTOM_OPS_STUB_H
#define GE_CUSTOM_OPS_STUB_H
#include <cstddef>
#include <cstdint>
#include <vector>

#include "graph/custom_op.h"

class MyCustomOp : public ge::BaseCustomOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  };
};

class MyPortableOp : public ge::PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &out) override {
    out = {1U, 2U, 3U};
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }
};

class MyPortableOpFailed : public ge::PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &out) override {
    out = {9U};
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_FAILED;
  }
};

REG_AUTO_MAPPING_OP(MyCustomOp);
REG_AUTO_MAPPING_OP(MyPortableOp);
#endif