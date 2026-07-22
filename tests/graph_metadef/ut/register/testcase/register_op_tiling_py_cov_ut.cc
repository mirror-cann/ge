/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "graph/ge_tensor.h"
#include "graph/utils/attr_utils.h"
#include "graph/op_desc.h"
#include "graph/operator.h"
#include "graph/compute_graph.h"
#include "graph/utils/op_desc_utils.h"
#include "register/op_tiling_registry.h"
#include "op_tiling/op_tiling_utils.h"
#include "op_tiling/op_tiling_constants.h"
#include "op_tiling.h"

using namespace std;
using namespace ge;

namespace optiling {

class RegisterOpTilingPyCovUT : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}
};

bool op_tiling_stub_py_v1(const TeOpParas &op_paras, const OpCompileInfo &compile_info, OpRunInfo &run_info) {
  return true;
}

bool op_tiling_stub_py_v2(const Operator &op, const utils::OpCompileInfo &compile_info, utils::OpRunInfo &run_info) {
  return true;
}

void *op_parse_stub_py_v3(const Operator &op, const ge::AscendString &compile_info_str) {
  static int32_t dummy = 0;
  return &dummy;
}

void *op_parse_stub_py_v4(const Operator &op, const ge::AscendString &compile_info_str) {
  static int32_t dummy = 0;
  return &dummy;
}

bool op_tiling_stub_py_v3(const Operator &op, void *compile_info, OpRunInfoV2 &run_info) {
  return true;
}

bool op_tiling_stub_py_v4(const Operator &op, const CompileInfoPtr &compile_info, OpRunInfoV2 &run_info) {
  return true;
}

extern "C" int TbeOpTilingPyInterface(const char *optype, const char *compile_info, const char *compile_info_hash,
                                      const char *inputs, const char *outputs, const char *attrs, char *run_info_json,
                                      size_t run_info_len, uint64_t *elapse);
extern "C" int TbeOpTilingPyInterfaceEx2(const char *optype, const char *compile_info, const char *inputs,
                                         const char *outputs, char *run_info_json, size_t run_info_len,
                                         const char *compile_info_hash, uint64_t *elapse);
extern "C" int OpTilingForCompile(const char *optype, const char *compile_info, const char *compile_info_hash,
                                  const char *inputs, const char *outputs, const char *attrs, char *run_info_json,
                                  size_t run_info_len, uint64_t *elapse, const char *extra_info);
extern "C" Status TbeLoadSoAndSaveToRegistry(const char *so_path);

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_NullOptype) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = TbeOpTilingPyInterface(nullptr, "", "", "", "", nullptr, run_info_json, sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_NullParams) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = TbeOpTilingPyInterface("Relu", nullptr, "", "", "", nullptr, run_info_json, sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_InvalidJson) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", "invalid_json", "invalid_json", nullptr,
                                   run_info_json, sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_EmptyInputsOutputs) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", "[]", "[]", nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ValidInputsOutputs) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ConstTensorInput) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"int32",)"
                       R"("const_value":[1,2,3,4],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ConstTensorFloat16) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float16",)"
                       R"("const_value":[1.0,2.0,3.0,4.0],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float16"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ConstTensorBF16) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"bfloat16",)"
                       R"("const_value":[1.0,2.0,3.0,4.0],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"bfloat16"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ConstTensorUnknownDtype) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs =
      R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"unknown_dtype",)"
      R"("const_value":[1,2,3,4],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_AutoTiling) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface(OP_TYPE_AUTO_TILING.c_str(), "compile_info", "hash", inputs, outputs, nullptr,
                                   run_info_json, sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterfaceEx2_NullParams) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = TbeOpTilingPyInterfaceEx2("Relu", nullptr, "", "", run_info_json, sizeof(run_info_json), "hash", elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterfaceEx2_InvalidJson) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = TbeOpTilingPyInterfaceEx2("Relu", "compile_info", "invalid", "invalid", run_info_json,
                                      sizeof(run_info_json), "hash", elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_NullOptype) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = OpTilingForCompile(nullptr, "", "", "", "", nullptr, run_info_json, sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_NullCompileInfo) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret =
      OpTilingForCompile("Relu", nullptr, "", "", "", nullptr, run_info_json, sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_NullInputs) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = OpTilingForCompile("Relu", "compile_info", "", nullptr, "", nullptr, run_info_json, sizeof(run_info_json),
                               elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_NullOutputs) {
  char run_info_json[1024] = {0};
  uint64_t elapse[2] = {0};
  int ret = OpTilingForCompile("Relu", "compile_info", "", "", nullptr, nullptr, run_info_json, sizeof(run_info_json),
                               elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_AutoTiling) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = OpTilingForCompile(OP_TYPE_AUTO_TILING.c_str(), "compile_info", "hash", inputs, outputs, nullptr,
                               run_info_json, sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_NormalOp) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithExtraInfo) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *extra_info = R"({"device_id":"0"})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, extra_info);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithAttrs) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"int","value":42}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithBoolAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"bool","value":true}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithStrAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"str","value":"test_value"}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithListAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_int","value":[1,2,3]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithListBoolAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_bool","value":[true,false]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithListStrAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_str","value":["a","b"]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithFloatAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"float","value":3.14}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithFloatAttrNullDesc) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"float","value":3.14,"value_null_desc":"inf"}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithListFloatAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_float","value":[1.1,2.2]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithListListIntAttr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_list_int","value":[[1,2],[3,4]]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithListListInt64Attr) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_list_int64","value":[[1,2],[3,4]]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithUnknownAttrType) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"unknown_type","value":42}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithConstTensorAllDtypes) {
  char run_info_json[8192] = {0};
  uint64_t elapse[2] = {0};
  const vector<string> dtypes = {"int8",  "uint8",  "int16",   "uint16", "int32",   "uint32",
                                 "int64", "uint64", "float32", "double", "float16", "bfloat16"};
  for (const auto &dtype : dtypes) {
    std::string inputs_str = R"([{"shape":[2],"ori_shape":[2],"format":"NCHW","ori_format":"NCHW","dtype":")" + dtype +
                             R"(","const_value":[1,2],"name":"x"}])";
    std::string outputs_str =
        R"([{"shape":[2],"ori_shape":[2],"format":"NCHW","ori_format":"NCHW","dtype":")" + dtype + R"("}])";
    int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs_str.c_str(), outputs_str.c_str(), nullptr,
                                 run_info_json, sizeof(run_info_json), elapse, nullptr);
    EXPECT_EQ(ret, 0);
  }
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithConstTensorNullDesc) {
  char run_info_json[8192] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[2],"ori_shape":[2],"format":"NCHW","ori_format":"NCHW","dtype":"float32",)"
                       R"("const_value":[null,2.0],"const_value_null_desc":["inf",null],"name":"x"}])";
  const char *outputs = R"([{"shape":[2],"ori_shape":[2],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithExtraInfoDeterministic) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *extra_info = R"({"deterministic":1,"deterministic_level":2})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, extra_info);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithExtraInfoAicoreNum) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *extra_info = R"({"_op_aicore_num":"8","_op_vectorcore_num":"4"})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, extra_info);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithExtraInfoHcomTopo) {
  char run_info_json[8192] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *extra_info =
      R"({"hcom_topo_info":{"rank_size":8,"local_window_size":4,)"
      R"("topo_level_descs":[{"comm_sets":"0,1,2,3","rank_size":4},{"comm_sets":"0,1","rank_size":2}]}})";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, extra_info);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithExtraInfoInvalidJson) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *extra_info = "invalid_json";
  int ret = OpTilingForCompile("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                               sizeof(run_info_json), elapse, extra_info);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithCompileInfoDeviceId) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *compile_info = R"({"device_id":"0","_cube_vector_core_type":"VecCore"})";
  int ret = OpTilingForCompile(OP_TYPE_AUTO_TILING.c_str(), compile_info, "hash", inputs, outputs, nullptr,
                               run_info_json, sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, OpTilingForCompile_WithCompileInfoCoreType) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *compile_info = R"({"_cube_vector_core_type":null,"_core_type_vec":"VecCore"})";
  int ret = OpTilingForCompile(OP_TYPE_AUTO_TILING.c_str(), compile_info, "hash", inputs, outputs, nullptr,
                               run_info_json, sizeof(run_info_json), elapse, nullptr);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeLoadSoAndSaveToRegistry_NullPath) {
  Status ret = TbeLoadSoAndSaveToRegistry(nullptr);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(RegisterOpTilingPyCovUT, TbeLoadSoAndSaveToRegistry_NonExistSo) {
  Status ret = TbeLoadSoAndSaveToRegistry("/nonexist/path/libtest.so");
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ArrayInputOutput) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([[{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}]])";
  const char *outputs =
      R"([[{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}]])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ConstTensorNoName) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs =
      R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"int32","const_value":[1,2]}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ConstTensorWithShapeOnly) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"dtype":"int32","const_value":[1,2,3,4],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_NullDescInf) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs =
      R"([{"shape":[1,4],"dtype":"float32","const_value":[null],"const_value_null_desc":["inf"],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_NullDescNegInf) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs =
      R"([{"shape":[1,4],"dtype":"float32","const_value":[null],"const_value_null_desc":["-inf"],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_NullDescNan) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs =
      R"([{"shape":[1,4],"dtype":"float32","const_value":[null],"const_value_null_desc":["nan"],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_NullDescInvalid) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs =
      R"([{"shape":[1,4],"dtype":"float32","const_value":[null],"const_value_null_desc":["invalid"],"name":"x"}])";
  const char *outputs = R"([{"shape":[1,4],"dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_FloatAttrWithNullDesc) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"float","value":null,"value_null_desc":"nan"}})";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_FloatAttrWithNonNullValueAndNullDesc) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"float","value":3.14,"value_null_desc":"nan"}})";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_ListFloatAttrWithNullDesc) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = R"({"attr_name":{"type":"list_float","value":[null,2.0],"value_null_desc":["inf",null]}})";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_AttrWithInvalidJson) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *attrs = "invalid_json";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, attrs, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_WithElapse) {
  char run_info_json[4096] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}

TEST_F(RegisterOpTilingPyCovUT, TbeOpTilingPyInterface_SmallRunInfoBuffer) {
  char run_info_json[4] = {0};
  uint64_t elapse[2] = {0};
  const char *inputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  const char *outputs = R"([{"shape":[1,4],"ori_shape":[1,4],"format":"NCHW","ori_format":"NCHW","dtype":"float32"}])";
  int ret = TbeOpTilingPyInterface("Relu", "compile_info", "hash", inputs, outputs, nullptr, run_info_json,
                                   sizeof(run_info_json), elapse);
  EXPECT_EQ(ret, 0);
}
}  // namespace optiling
