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
#include <cstdint>
#include <vector>

#include "graph/custom_op.h"

using namespace ge;

class MyEagerExecuteOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    return GRAPH_SUCCESS;
  }
};

class MyCompilableOp: public EagerExecuteOp, public CompilableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
  graphStatus Compile(gert::OpCompileContext *ctx) override {
    (void)ctx;
    stub_compiled_path = "stub_compiled_path";
    return GRAPH_SUCCESS;
  }
  graphStatus GetStubCompiledPath(std::string &path) const {
    path = stub_compiled_path;
    return GRAPH_SUCCESS;
  }

 private:
  std::string stub_compiled_path;
};

class MyPortableOp : public EagerExecuteOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    return GRAPH_SUCCESS;
  }
  graphStatus Serialize(std::vector<uint8_t> &out) override {
    out = {1U, 2U, 3U};
    return ge::GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_SUCCESS;
  }
};

class MyPortableOpFailed : public EagerExecuteOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  graphStatus Serialize(std::vector<uint8_t> &out) override {
    out = {9U};
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_FAILED;
  }
};

REG_AUTO_MAPPING_OP(MyEagerExecuteOp);
REG_AUTO_MAPPING_OP(MyCompilableOp);
REG_AUTO_MAPPING_OP(MyPortableOp);
REG_AUTO_MAPPING_OP(MyPortableOpFailed);
#endif