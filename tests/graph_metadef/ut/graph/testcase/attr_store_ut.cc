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
#include "graph/attr_store.h"
#include "graph/op_desc.h"
#include "ge_common/debug/ge_log.h"
#include "graph/attribute_group/attr_group_serialize.h"
#include "graph/attribute_group/attr_group_base.h"
#include "graph/attribute_group/attr_group_serializer_registry.h"
#include "graph/compute_graph.h"
#include "graph/attribute_group/attr_group_shape_env.h"
#include "graph_metadef/graph/debug/ge_util.h"
#include "common/checker.h"
#include "test_structs.h"

namespace ge {
namespace {
struct TestStructB {
  int16_t a;
  int b;
  int64_t c;
  bool operator==(const TestStructB &other) const {
    return a == other.a && b == other.b && c == other.c;
  }
};

}
class AttrStoreUt : public testing::Test {
};

TEST_F(AttrStoreUt, CreateAndGetOk1) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<bool>(0, true));
  EXPECT_TRUE(s.Set<bool>(1, false));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_NE(s.Get<bool>(0), nullptr);
  EXPECT_TRUE(*s.Get<bool>(0));
  EXPECT_NE(s.Get<bool>(1), nullptr);
  EXPECT_FALSE(*s.Get<bool>(1));

  EXPECT_NE(s.GetByName<bool>("transpose_x1"), nullptr);
  EXPECT_TRUE(*s.GetByName<bool>("transpose_x1"));
  EXPECT_NE(s.GetByName<bool>("transpose_x2"), nullptr);
  EXPECT_FALSE(*s.GetByName<bool>("transpose_x2"));
}

TEST_F(AttrStoreUt, CreateAndGetOk2) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set(0, true));
  EXPECT_TRUE(s.Set(1, false));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_NE(s.Get<bool>(0), nullptr);
  EXPECT_TRUE(*s.Get<bool>(0));
  EXPECT_NE(s.Get<bool>(1), nullptr);
  EXPECT_FALSE(*s.Get<bool>(1));
}

TEST_F(AttrStoreUt, CreateAndGetOk3) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set(0, true));
  EXPECT_TRUE(s.Set(1, TestStructB({1,2,3})));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_NE(s.Get<bool>(0), nullptr);
  EXPECT_TRUE(*s.Get<bool>(0));
  EXPECT_NE(s.Get<TestStructB>(1), nullptr);
  EXPECT_EQ(*s.Get<TestStructB>(1), TestStructB({1,2,3}));

  EXPECT_NE(s.GetByName<bool>("transpose_x1"), nullptr);
  EXPECT_TRUE(*s.GetByName<bool>("transpose_x1"));
  EXPECT_NE(s.GetByName<TestStructB>("transpose_x2"), nullptr);
  EXPECT_EQ(*s.GetByName<TestStructB>("transpose_x2"), TestStructB({1,2,3}));
}

TEST_F(AttrStoreUt, CreateAndGetOk_RLValue1) {
  int a = 10;
  int &b = a;
  const int c = 20;

  auto s = AttrStore::Create(4);
  EXPECT_TRUE(s.Set(0, a));
  EXPECT_TRUE(s.Set(1, b));
  EXPECT_TRUE(s.Set(2, c));
  EXPECT_TRUE(s.Set(3, 20));

  EXPECT_EQ(*s.Get<int>(0), 10);
  EXPECT_EQ(*s.Get<int>(1), 10);
  EXPECT_EQ(*s.Get<int>(2), 20);
  EXPECT_EQ(*s.Get<int>(3), 20);
}

TEST_F(AttrStoreUt, CreateAndGetOk_RLValue2) {
  TestStructB a = {10, 20, 30};
  TestStructB &b = a;
  const TestStructB c = {100, 200, 300};

  auto s = AttrStore::Create(4);
  EXPECT_TRUE(s.SetByName("attr_0", a));
  EXPECT_TRUE(s.SetByName("attr_1", b));
  EXPECT_TRUE(s.SetByName("attr_2", c));
  EXPECT_TRUE(s.SetByName("attr_3", TestStructB{100,200,300}));

  EXPECT_EQ(*s.GetByName<TestStructB>("attr_0"), a);
  EXPECT_EQ(*s.GetByName<TestStructB>("attr_1"), a);
  EXPECT_EQ(*s.GetByName<TestStructB>("attr_2"), c);
  EXPECT_EQ(*s.GetByName<TestStructB>("attr_3"), c);
}

TEST_F(AttrStoreUt, CreateAndGetByNameOk1) {
  auto s = AttrStore::Create(2);

  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_TRUE(s.SetByName("transpose_x1", true));
  EXPECT_TRUE(s.SetByName("transpose_x2", false));

  EXPECT_NE(s.Get<bool>(0), nullptr);
  EXPECT_TRUE(*s.Get<bool>(0));
  EXPECT_NE(s.Get<bool>(1), nullptr);
  EXPECT_FALSE(*s.Get<bool>(1));

  EXPECT_NE(s.GetByName<bool>("transpose_x1"), nullptr);
  EXPECT_TRUE(*s.GetByName<bool>("transpose_x1"));
  EXPECT_NE(s.GetByName<bool>("transpose_x2"), nullptr);
  EXPECT_FALSE(*s.GetByName<bool>("transpose_x2"));
}

TEST_F(AttrStoreUt, CreateAndGetByNameOk2) {
  auto s = AttrStore::Create(2);

  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_TRUE(s.SetByName("transpose_x3", true));
  EXPECT_TRUE(s.SetByName("transpose_x4", false));

  EXPECT_EQ(s.Get<bool>(0), nullptr);
  EXPECT_EQ(s.Get<bool>(1), nullptr);

  EXPECT_NE(s.GetByName<bool>("transpose_x3"), nullptr);
  EXPECT_NE(s.GetByName<bool>("transpose_x4"), nullptr);

  EXPECT_EQ(*s.GetByName<bool>("transpose_x3"), true);
  EXPECT_EQ(*s.GetByName<bool>("transpose_x4"), false);
}

TEST_F(AttrStoreUt, GetNotExists) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<bool>(0, true));
  EXPECT_TRUE(s.Set<bool>(1, false));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_EQ(s.Get<bool>(2), nullptr);
  EXPECT_EQ(s.MutableGet<bool>(2), nullptr);

  EXPECT_EQ(s.GetByName<bool>("transpose_x3"), nullptr);
  EXPECT_EQ(s.MutableGetByName<bool>("transpose_x3"), nullptr);
}

TEST_F(AttrStoreUt, DeleteOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<bool>(0, true));
  EXPECT_TRUE(s.Set<bool>(1, false));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_NE(s.Get<bool>(0), nullptr);
  EXPECT_NE(s.Get<bool>(1), nullptr);

  EXPECT_TRUE(s.Delete("transpose_x1"));
  EXPECT_EQ(s.Get<bool>(0), nullptr);
  EXPECT_FALSE(s.Delete("transpose_x1"));
}

TEST_F(AttrStoreUt, GetWithWrongType) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<bool>(0, true));
  EXPECT_TRUE(s.Set<TestStructB>(1, {1,2,10}));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_NE(s.Get<bool>(0), nullptr);
  EXPECT_NE(s.Get<TestStructB>(1), nullptr);
  EXPECT_EQ(s.Get<int>(0), nullptr);
  EXPECT_EQ(s.Get<int>(1), nullptr);
}

TEST_F(AttrStoreUt, ModifyOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<bool>(0, true));
  EXPECT_TRUE(s.Set<bool>(1, false));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_TRUE(*s.Get<bool>(0));
  EXPECT_FALSE(*s.Get<bool>(1));

  EXPECT_TRUE(s.Set<bool>(0, false));
  EXPECT_FALSE(*s.Get<bool>(0));
}

TEST_F(AttrStoreUt, ModifyByNameOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<int64_t>(0, 100));
  EXPECT_TRUE(s.Set<int64_t>(1, 200));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  auto p = s.MutableGetByName<int64_t>("transpose_x1");
  EXPECT_NE(p,  nullptr);
  *p = 101;
  EXPECT_EQ(*s.Get<int64_t>(0), 101);
  EXPECT_EQ(*s.GetByName<int64_t>("transpose_x1"), 101);


  p = s.MutableGetByName<int64_t>("transpose_x2");
  EXPECT_NE(p,  nullptr);
  *p = 201;
  EXPECT_EQ(*s.Get<int64_t>(1), 201);
  EXPECT_EQ(*s.GetByName<int64_t>("transpose_x2"), 201);
}

TEST_F(AttrStoreUt, ExistsOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<int64_t>(0, 100));
  EXPECT_TRUE(s.Set<int64_t>(1, 200));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);

  EXPECT_TRUE(s.Exists(0));
  EXPECT_TRUE(s.Exists(1));
  EXPECT_TRUE(s.Exists("transpose_x1"));
  EXPECT_TRUE(s.Exists("transpose_x2"));
  EXPECT_FALSE(s.Exists(2));
  EXPECT_FALSE(s.Exists("transpose_x3"));
}


TEST_F(AttrStoreUt, GetAllAttrNamesOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<int64_t>(0, 100));
  EXPECT_TRUE(s.Set<bool>(1, true));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);
  s.SetByName("attr_3", TestStructB{10,200,3000});
  s.SetByName("attr_4", std::vector<int64_t>({1,2,3,4,5}));

  EXPECT_EQ(s.GetAllAttrNames(), std::set<std::string>({"transpose_x1",
                                                        "transpose_x2",
                                                        "attr_3",
                                                        "attr_4"}));
}

TEST_F(AttrStoreUt, GetAllAttrsOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<int64_t>(0, 100));
  EXPECT_TRUE(s.Set<bool>(1, true));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);
  s.SetByName("attr_3", TestStructB{10,200,3000});
  s.SetByName("attr_4", std::vector<int64_t>({1,2,3,4,5}));

  auto attrs = s.GetAllAttrs();
  EXPECT_EQ(attrs.size(), 4);
  EXPECT_EQ(*attrs["transpose_x1"].Get<int64_t>(), 100);
  EXPECT_EQ(*attrs["transpose_x2"].Get<bool>(), true);
  EXPECT_EQ(*attrs["attr_3"].Get<TestStructB>(), TestStructB({10,200,3000}));
  EXPECT_EQ(*attrs["attr_4"].Get<std::vector<int64_t>>(), std::vector<int64_t>({1,2,3,4,5}));
}

TEST_F(AttrStoreUt, GetAllAttrs_EmptyPredefinedAttrsNotReturn) {
  auto s = AttrStore::Create(2);
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);
  EXPECT_TRUE(s.Set<int64_t>(0, 100));
  s.SetByName("attr_3", TestStructB{10,200,3000});
  s.SetByName("attr_4", std::vector<int64_t>({1,2,3,4,5}));

  auto attrs = s.GetAllAttrs();
  EXPECT_EQ(attrs.size(), 3);
  EXPECT_EQ(attrs.count("transpose_x2"), 0);
}

TEST_F(AttrStoreUt, GetAllAttrsWithFilterOk) {
  auto s = AttrStore::Create(2);
  EXPECT_TRUE(s.Set<int64_t>(0, 100));
  EXPECT_TRUE(s.Set<bool>(1, true));
  s.SetNameAndId("transpose_x1", 0);
  s.SetNameAndId("transpose_x2", 1);
  s.SetByName("attr_3", TestStructB{10,200,3000});
  s.SetByName("attr_4", std::vector<int64_t>({1,2,3,4,5}));

  const AttrNameFilter attr_filter = [](const std::string &attr_name) -> bool {
    return (attr_name != "transpose_x1") && (attr_name != "attr_4");
  };

  auto attrs = s.GetAllAttrsWithFilter(attr_filter);
  EXPECT_EQ(attrs.size(), 2);
  EXPECT_EQ(*attrs["transpose_x2"].Get<bool>(), true);
  EXPECT_EQ(*attrs["attr_3"].Get<TestStructB>(), TestStructB({10,200,3000}));
}


TEST_F(AttrStoreUt, OtherAttrGroupTest) {
  auto s = AttrStore::Create(1);
  std::string attr_name = "Max memory";
  auto flag = s.CheckAttrIsExistInOtherGroup(attr_name);
  ASSERT_EQ(flag, false);

  AnyValue any_value;
  auto ret = s.GetAttrFromOtherGroup(attr_name, any_value);
  ASSERT_EQ(ret, GRAPH_FAILED);

  any_value.SetValue<int64_t>(1);

  ret = s.SetAttrToOtherGroup(attr_name, any_value);
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  AnyValue any_value1;
  ret = s.GetAttrFromOtherGroup(attr_name, any_value1);
  ASSERT_EQ(ret, GRAPH_SUCCESS);
  int64_t value = -1;
  int64_t value1 = -1;
  any_value.GetValue(value);
  any_value1.GetValue(value1);
  ASSERT_EQ(value, 1);
  ASSERT_EQ(value, value1);

  ret = s.GetAttrFromOtherGroup(attr_name, any_value1);
  ASSERT_EQ(ret, GRAPH_SUCCESS);
  value = -1;
  value1 = -1;
  any_value.GetValue(value);
  any_value1.GetValue(value1);
  ASSERT_EQ(value, 1);
  ASSERT_EQ(value, value1);

  bool delete_flag = s.DeleteSingleAttrsInOtherGroup(attr_name);
  ASSERT_EQ(delete_flag, true);

  ret = s.GetAttrFromOtherGroup(attr_name, any_value1);
  ASSERT_EQ(ret, GRAPH_FAILED);

  any_value.SetValue<int64_t>(10);
  ret = s.SetAttrToOtherGroup(attr_name, any_value);
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  ret = s.GetAttrFromOtherGroup(attr_name, any_value1);
  ASSERT_EQ(ret, GRAPH_SUCCESS);
  value = -1;
  value1 = -1;
  any_value.GetValue(value);
  any_value1.GetValue(value1);
  ASSERT_EQ(value, 10);
  ASSERT_EQ(value, value1);

  s.DeleteAllAttrsInOtherGroup();

  ret = s.GetAttrFromOtherGroup(attr_name, any_value1);
  ASSERT_EQ(ret, GRAPH_FAILED);
}

TEST_F(AttrStoreUt, ErrorTest) {
  auto s = AttrStore::Create(1);
  std::string no_exist_attr_name = "TEST2";

  AnyValue any_value;
  auto ret = s.SetAttrToOtherGroup(no_exist_attr_name, any_value);
  ASSERT_EQ(ret, GRAPH_FAILED);

  std::string attr_name = "Max memory";
  ret = s.SetAttrToOtherGroup(attr_name, any_value);
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  auto flag = s.CheckAttrIsExistInOtherGroup(attr_name);
  ASSERT_EQ(flag, true);
}


TEST_F(AttrStoreUt, GetOrCreateAttrGroupWith0Args) {
  auto s = AttrStore::Create(1);
  auto ptr = s.CreateAttrsGroup<TestAttrGroup>();
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(ptr->a, 0);
  ASSERT_EQ(ptr->b, 0);

  auto ptr_1 = s.CreateAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_1, nullptr);

  auto ptr_2 = s.CreateAttrsGroup<TestAttrGroup>(1);
  ASSERT_EQ(ptr_2, nullptr);

  auto ptr_3 = s.CreateAttrsGroup<TestAttrGroup>(1, 2);
  ASSERT_EQ(ptr_3, nullptr);

  auto ptr_4 = s.GetAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_4, ptr);

  auto ptr_5 = s.GetOrCreateAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_5, ptr);
}

TEST_F(AttrStoreUt, GetOrCreateAttrGroupWith1Args) {
  auto s = AttrStore::Create(1);
  auto ptr = s.CreateAttrsGroup<TestAttrGroup>(1);
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(ptr->a, 1);
  ASSERT_EQ(ptr->b, 0);

  auto ptr_1 = s.CreateAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_1, nullptr);

  auto ptr_2 = s.CreateAttrsGroup<TestAttrGroup>(1);
  ASSERT_EQ(ptr_2, nullptr);

  auto ptr_3 = s.CreateAttrsGroup<TestAttrGroup>(1, 2);
  ASSERT_EQ(ptr_3, nullptr);

  auto ptr_4 = s.GetAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_4, ptr);

  auto ptr_5 = s.GetOrCreateAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_5, ptr);
}

TEST_F(AttrStoreUt, DeleteAttrsGroup) {
  auto s = AttrStore::Create(1);
  ASSERT_FALSE(s.DeleteAttrsGroup<TestAttrGroup>());
  s.CreateAttrsGroup<TestAttrGroup>();
  ASSERT_TRUE(s.DeleteAttrsGroup<TestAttrGroup>());
  ASSERT_FALSE(s.DeleteAttrsGroup<TestAttrGroup>());
  ASSERT_EQ(s.GetAttrsGroup<TestAttrGroup>(), nullptr);
  ASSERT_FALSE(s.CheckAttrGroupIsExist<TestAttrGroup>());
  s.CreateAttrsGroup<TestAttrGroup>();
  ASSERT_TRUE(s.DeleteAttrsGroup<TestAttrGroup>());

  ge::ComputeGraph cg("simple");
  auto graph_attr_group_ptr = cg.GetOrCreateAttrsGroup<ShapeEnvAttr>();
  ASSERT_TRUE(cg.DeleteAttrsGroup<ShapeEnvAttr>());
  ASSERT_EQ(cg.GetAttrsGroup<ShapeEnvAttr>(), nullptr);
  ASSERT_FALSE(cg.DeleteAttrsGroup<ShapeEnvAttr>());
  graph_attr_group_ptr = cg.CreateAttrsGroup<ShapeEnvAttr>();
  EXPECT_TRUE(graph_attr_group_ptr != nullptr);
}

TEST_F(AttrStoreUt, GetOrCreateAttrGroupWith2Args) {
  auto s = AttrStore::Create(1);
  auto ptr = s.CreateAttrsGroup<TestAttrGroup>(1, 2);
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(ptr->a, 1);
  ASSERT_EQ(ptr->b, 2);

  auto ptr_1 = s.CreateAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_1, nullptr);

  auto ptr_2 = s.CreateAttrsGroup<TestAttrGroup>(1);
  ASSERT_EQ(ptr_2, nullptr);

  auto ptr_3 = s.CreateAttrsGroup<TestAttrGroup>(1, 2);
  ASSERT_EQ(ptr_3, nullptr);

  auto ptr_4 = s.GetAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_4, ptr);

  auto ptr_5 = s.GetOrCreateAttrsGroup<TestAttrGroup>();
  ASSERT_EQ(ptr_5, ptr);
}
}
