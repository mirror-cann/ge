/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/emitter/cpp_emitter.h"
#include "common/om2/codegen/ast/ast_context.h"
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "common/om2/codegen/emitter/stable_parts/stable_part_provider.h"
#include "common/helper/om2/om2_utils.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

namespace ge {
namespace {
class ScopedEnvVar {
 public:
  ScopedEnvVar(const char *name, const char *value) : name_(name) {
    const char *old_value = getenv(name);
    if (old_value != nullptr) {
      old_value_ = old_value;
      has_old_value_ = true;
    }
    (void)setenv(name, value, 1);
  }

  ~ScopedEnvVar() {
    if (has_old_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  std::string old_value_;
  bool has_old_value_ = false;
};

class ScopedStdoutCapture {
 public:
  ScopedStdoutCapture() {
    capture_file_ = tmpfile();
    if (capture_file_ == nullptr) {
      return;
    }
    saved_stdout_fd_ = dup(STDOUT_FILENO);
    if (saved_stdout_fd_ < 0) {
      CloseCaptureFile();
      return;
    }
    (void)fflush(stdout);
    if (dup2(fileno(capture_file_), STDOUT_FILENO) < 0) {
      CloseSavedStdout();
      CloseCaptureFile();
    }
  }

  ~ScopedStdoutCapture() {
    (void)Stop();
  }

  std::string Stop() {
    if (stopped_) {
      return output_;
    }
    stopped_ = true;
    if (saved_stdout_fd_ >= 0) {
      (void)fflush(stdout);
      (void)dup2(saved_stdout_fd_, STDOUT_FILENO);
      CloseSavedStdout();
    }
    if (capture_file_ != nullptr) {
      (void)fflush(capture_file_);
      (void)fseek(capture_file_, 0, SEEK_SET);
      char buffer[4096];
      while (!feof(capture_file_)) {
        const size_t read_size = fread(buffer, 1, sizeof(buffer), capture_file_);
        output_.append(buffer, read_size);
      }
      CloseCaptureFile();
    }
    return output_;
  }

 private:
  void CloseSavedStdout() {
    if (saved_stdout_fd_ >= 0) {
      (void)close(saved_stdout_fd_);
      saved_stdout_fd_ = -1;
    }
  }

  void CloseCaptureFile() {
    if (capture_file_ != nullptr) {
      (void)fclose(capture_file_);
      capture_file_ = nullptr;
    }
  }

  FILE *capture_file_ = nullptr;
  int32_t saved_stdout_fd_ = -1;
  bool stopped_ = false;
  std::string output_;
};

std::string EmitNode(const AstNode &node) {
  CppEmitter emitter;
  std::string output;
  EXPECT_EQ(node.Accept(emitter, output), SUCCESS);
  return output;
}

void ExpectContainsAll(const std::string &output, const std::vector<std::string> &snippets) {
  for (const auto &snippet : snippets) {
    EXPECT_NE(output.find(snippet), std::string::npos) << snippet << "\n=== output ===\n" << output;
  }
}
}  // namespace

class Om2CodegenUt : public testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(Om2CodegenUt, AstNodes_AllPublicInterfaces_Ok) {
  AstContext ctx;

  auto *param_x = ParamDecl::Create(ctx, "int", "x");
  auto *param_y = ParamDecl::Create(ctx, "int", "y");
  ASSERT_NE(param_x, nullptr);
  ASSERT_NE(param_y, nullptr);
  EXPECT_EQ(std::string(param_x->GetTypeSpec().Data(), param_x->GetTypeSpec().Length()), "int");
  EXPECT_EQ(std::string(param_x->GetName().Data(), param_x->GetName().Length()), "x");

  auto *ident_x = IdentifierExpr::Create(ctx, "x");
  auto *ident_y = IdentifierExpr::Create(ctx, "y");
  auto *ident_obj = IdentifierExpr::Create(ctx, "obj");
  auto *ident_ptr = IdentifierExpr::Create(ctx, "ptr");
  auto *ident_vec = IdentifierExpr::Create(ctx, "vec");
  auto *ident_consume = IdentifierExpr::Create(ctx, "Consume");
  auto *ident_runner = IdentifierExpr::Create(ctx, "runner");
  auto *ident_guard = IdentifierExpr::Create(ctx, "guard");
  auto *lit_int = LiteralExpr::CreateInt(ctx, 7, LiteralExpr::IntSuffix::kU);
  auto *lit_bool = LiteralExpr::CreateBool(ctx, true);
  auto *lit_str = LiteralExpr::CreateString(ctx, "txt");
  auto *lit_null = LiteralExpr::CreateNullptr(ctx);
  ASSERT_NE(ident_x, nullptr);
  ASSERT_NE(lit_int, nullptr);
  EXPECT_EQ(lit_int->GetKind(), LiteralExpr::Kind::kInt);
  EXPECT_EQ(lit_int->GetIntValue(), 7);
  EXPECT_EQ(lit_int->GetIntSuffix(), LiteralExpr::IntSuffix::kU);
  EXPECT_EQ(lit_bool->GetKind(), LiteralExpr::Kind::kBool);
  EXPECT_TRUE(lit_bool->GetBoolValue());
  EXPECT_EQ(lit_str->GetKind(), LiteralExpr::Kind::kString);
  EXPECT_EQ(std::string(lit_str->GetStringValue().Data(), lit_str->GetStringValue().Length()), "txt");
  EXPECT_EQ(lit_null->GetKind(), LiteralExpr::Kind::kNullptr);

  auto *assign_expr = AssignExpr::Create(ctx, ident_x, lit_int);
  auto *binary_add = BinaryExpr::Create(ctx, BinaryExpr::Op::kAdd, ident_x, ident_y);
  auto *binary_eq = BinaryExpr::Create(ctx, BinaryExpr::Op::kEq, ident_x, lit_int);
  auto *unary_not = UnaryExpr::Create(ctx, UnaryExpr::Op::kLogicalNot, binary_eq);
  auto *call_expr = CallExpr::Create(ctx, ident_consume, {ident_x, lit_int});
  auto *addr_expr = AddrOfExpr::Create(ctx, ident_x);
  auto *subscript_expr = SubscriptExpr::Create(ctx, ident_vec, lit_int);
  auto *member_expr = MemberExpr::Create(ctx, ident_obj, "field");
  auto *arrow_expr = CppArrowMemberExpr::Create(ctx, ident_ptr, "field");
  auto *reinterpret_cast_expr = CppCastExpr::Create(ctx, CppCastExpr::Kind::kReinterpret, "void *", ident_ptr);
  auto *static_cast_expr = CppCastExpr::Create(ctx, CppCastExpr::Kind::kStatic, "uint32_t", ident_x);
  auto *init_list_expr = InitListExpr::Create(ctx, {lit_int, lit_bool, lit_null});
  auto *lambda_body = BlockStmt::Create(ctx, {ReturnStmt::Create(ctx, ident_x)});
  auto *lambda_expr = LambdaExpr::Create(ctx, {"x", "&y"}, lambda_body);
  ASSERT_NE(assign_expr, nullptr);
  ASSERT_EQ(assign_expr->GetLhs(), ident_x);
  ASSERT_EQ(assign_expr->GetRhs(), lit_int);
  ASSERT_EQ(binary_add->GetOp(), BinaryExpr::Op::kAdd);
  ASSERT_EQ(binary_add->GetLhs(), ident_x);
  ASSERT_EQ(binary_add->GetRhs(), ident_y);
  ASSERT_EQ(unary_not->GetOp(), UnaryExpr::Op::kLogicalNot);
  ASSERT_EQ(call_expr->GetCallee(), ident_consume);
  ASSERT_EQ(call_expr->GetArgs().Size(), 2U);
  ASSERT_EQ(addr_expr->GetExpr(), ident_x);
  ASSERT_EQ(subscript_expr->GetBase(), ident_vec);
  ASSERT_EQ(subscript_expr->GetIndex(), lit_int);
  ASSERT_EQ(member_expr->GetObject(), ident_obj);
  ASSERT_EQ(arrow_expr->GetObject(), ident_ptr);
  ASSERT_EQ(reinterpret_cast_expr->GetKind(), CppCastExpr::Kind::kReinterpret);
  ASSERT_EQ(static_cast_expr->GetKind(), CppCastExpr::Kind::kStatic);
  ASSERT_EQ(init_list_expr->GetElements().Size(), 3U);
  ASSERT_EQ(lambda_expr->GetCaptures().Size(), 2U);
  ASSERT_EQ(lambda_expr->GetBody(), lambda_body);

  auto *comment_stmt = CommentStmt::Create(ctx, "comment");
  auto *blank_stmt = BlankLineStmt::Create(ctx);
  auto *var_decl_stmt = VarDeclStmt::Create(ctx, "int", "sum", binary_add);
  auto *expr_stmt = ExprStmt::Create(ctx, assign_expr);
  auto *return_stmt = ReturnStmt::Create(ctx, init_list_expr);
  auto *return_void_stmt = ReturnStmt::Create(ctx);
  auto *then_block = BlockStmt::Create(ctx, {comment_stmt, blank_stmt, var_decl_stmt, expr_stmt});
  auto *else_block = BlockStmt::Create(ctx, {ExprStmt::Create(ctx, call_expr), return_void_stmt});
  auto *if_stmt = IfStmt::Create(ctx, unary_not, then_block, else_block);
  auto *method_body = BlockStmt::Create(ctx, {ExprStmt::Create(ctx, member_expr), ExprStmt::Create(ctx, arrow_expr)});
  auto *func_body = BlockStmt::Create(ctx, {if_stmt, ExprStmt::Create(ctx, addr_expr), ExprStmt::Create(ctx, subscript_expr),
                                            ExprStmt::Create(ctx, reinterpret_cast_expr), ExprStmt::Create(ctx, static_cast_expr),
                                            ExprStmt::Create(ctx, lambda_expr), return_stmt});
  ASSERT_NE(comment_stmt, nullptr);
  ASSERT_EQ(std::string(comment_stmt->GetText().Data(), comment_stmt->GetText().Length()), "comment");
  ASSERT_EQ(var_decl_stmt->GetInit(), binary_add);
  ASSERT_EQ(expr_stmt->GetExpr(), assign_expr);
  ASSERT_EQ(return_stmt->GetValue(), init_list_expr);
  ASSERT_EQ(return_void_stmt->GetValue(), nullptr);
  ASSERT_EQ(then_block->GetStatements().Size(), 4U);
  ASSERT_EQ(if_stmt->GetCond(), unary_not);
  ASSERT_EQ(if_stmt->GetThenBlock(), then_block);
  ASSERT_EQ(if_stmt->GetElseBlock(), else_block);

  auto *field_decl = FieldDecl::Create(ctx, "int", "value", lit_int);
  auto *type_alias = TypeAliasDecl::Create(ctx, "void *", "Handle");
  auto *method_decl = MethodDecl::Create(ctx, "Run", {param_x}, "void");
  auto *function_decl = FunctionDecl::Create(ctx, "Add", {param_x, param_y}, "int");
  auto *function_def = FunctionDef::Create(ctx, "Build", {param_x, param_y}, "int", func_body);
  auto *method_def = MethodDef::Create(ctx, "Worker", "Exec", {param_x}, "void", {}, {}, method_body);
  auto *access_decl = AccessSectionDecl::Create(ctx, AccessSectionDecl::Kind::kPublic);
  auto *private_decl = AccessSectionDecl::Create(ctx, AccessSectionDecl::Kind::kPrivate);
  auto *class_decl =
      ClassDecl::Create(ctx, "Worker", {access_decl, field_decl, method_decl, private_decl,
                                        FieldDecl::Create(ctx, "int", "hidden")});
  auto *struct_decl = StructDecl::Create(ctx, "Pod", {FieldDecl::Create(ctx, "bool", "ready", lit_bool)});
  auto *extern_decl = ExternBlockDecl::Create(ctx, "C", {function_decl});
  auto *namespace_decl = NamespaceDecl::Create(ctx, "om2", {class_decl, struct_decl, function_def, method_def, extern_decl});
  auto *stable_part = StablePartDecl::Create(ctx, StablePartId::kChkStatusMacro, StablePartRole::kMacroGroup,
                                             StablePartPlacement::kTranslationUnit);
  auto *include_decl = IncludeDecl::Create(ctx, "vector", IncludeDecl::Kind::kAngle);
  auto *space_decl = SpaceDecl::Create(ctx);
  auto *tu = TranslationUnit::Create(ctx, {include_decl, space_decl, stable_part, type_alias, namespace_decl});
  ASSERT_NE(field_decl, nullptr);
  ASSERT_NE(type_alias, nullptr);
  ASSERT_EQ(field_decl->GetInit(), lit_int);
  EXPECT_EQ(std::string(type_alias->GetTypeSpec().Data(), type_alias->GetTypeSpec().Length()), "void *");
  EXPECT_EQ(std::string(type_alias->GetName().Data(), type_alias->GetName().Length()), "Handle");
  ASSERT_EQ(method_decl->GetParams().Size(), 1U);
  ASSERT_EQ(function_decl->GetParams().Size(), 2U);
  ASSERT_EQ(function_def->GetBody(), func_body);
  ASSERT_EQ(method_def->GetBody(), method_body);
  ASSERT_EQ(class_decl->GetItems().Size(), 5U);
  ASSERT_EQ(struct_decl->GetItems().Size(), 1U);
  ASSERT_EQ(extern_decl->GetItems().Size(), 1U);
  ASSERT_EQ(namespace_decl->GetItems().Size(), 5U);
  ASSERT_EQ(stable_part->GetId(), StablePartId::kChkStatusMacro);
  ASSERT_EQ(stable_part->GetRole(), StablePartRole::kMacroGroup);
  ASSERT_EQ(stable_part->GetPlacement(), StablePartPlacement::kTranslationUnit);
  ASSERT_EQ(include_decl->GetKind(), IncludeDecl::Kind::kAngle);
  ASSERT_NE(space_decl, nullptr);
  ASSERT_EQ(tu->GetItems().Size(), 5U);

  const auto output = EmitNode(*tu);
  ExpectContainsAll(output, {
      "#include <vector>\n\n",
      "#define OM2_CHK_STATUS",
      "typedef void *Handle;\n",
      "namespace om2 {\n",
      "class Worker {\n",
      "  public:\n",
      "    int value = 7U;\n",
      "    void Run(int x);\n",
      "  private:\n",
      "    int hidden;\n",
      "struct Pod {\n",
      "bool ready = true;\n",
      "int Build(int x, int y) {\n",
      "if (!((x == 7U)))",
      "// comment\n",
      "int sum = (x + y);\n",
      "x = 7U;\n",
      "Consume(x, 7U);\n",
      "return {7U, true, nullptr};\n",
      "void Worker::Exec(int x) {\n",
      "obj.field;\n",
      "ptr->field;\n",
      "extern \"C\" {\n",
      "int Add(int x, int y);\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_AllPublicInterfaces_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  Arg empty_arg;
  EXPECT_TRUE(empty_arg.Empty());
  EXPECT_EQ(empty_arg.Resolve(ctx), nullptr);
  Arg ident_from_cstr("symbol_from_cstr");
  Arg ident_from_string(std::string("symbol_from_string"));
  Arg int_arg(3);
  Arg bool_arg(false);
  Arg null_arg(nullptr);
  Arg string_lit = Arg::StringLiteral("string_literal");
  EXPECT_FALSE(ident_from_cstr.Empty());

  auto lhs = ast.Var("uint32_t", "lhs");
  auto rhs = ast.Var("uint32_t", "rhs");
  auto ptr = ast.Var("Node *", "ptr");
  auto obj = ast.Var("Node", "obj");
  auto arr = ast.Var("std::vector<uint32_t>", "arr");
  auto runner = ast.Var("Runner", "runner");

  EXPECT_EQ(lhs.TypeName(), "uint32_t");
  EXPECT_EQ(lhs.SymbolName(), "lhs");
  ASSERT_NE(lhs.Get(), nullptr);
  ASSERT_NE(lhs.Ref().Get(), nullptr);
  ASSERT_NE(lhs.Addr().Get(), nullptr);
  ASSERT_NE(lhs.Attr("size").Get(), nullptr);
  ASSERT_NE(ptr.Arrow("field").Get(), nullptr);
  ASSERT_NE(arr[1].Get(), nullptr);
  ASSERT_NE(runner().Get(), nullptr);
  ASSERT_NE(runner({lhs}).Get(), nullptr);
  ASSERT_NE(runner(lhs, rhs).Get(), nullptr);

  static_assert(std::is_same<decltype(obj.Ref().Attr("field")), ExprRef>::value,
                "Attr should directly materialize a member expression");
  static_assert(std::is_same<decltype(ptr.Ref().Arrow("field")), ExprRef>::value,
                "Arrow should directly materialize an arrow member expression");
  auto expr_attr = obj.Ref().Attr("field");
  auto expr_arrow = ptr.Ref().Arrow("field");
  ASSERT_NE(expr_attr.Get(), nullptr);
  ASSERT_NE(expr_arrow.Get(), nullptr);
  ASSERT_NE(expr_attr.Addr().Get(), nullptr);
  ASSERT_NE(expr_attr().Get(), nullptr);
  ASSERT_NE(expr_attr({lhs}).Get(), nullptr);
  ASSERT_NE(expr_attr(lhs, rhs).Get(), nullptr);
  ASSERT_NE(obj.Ref().Addr().Get(), nullptr);
  ASSERT_NE(obj.Ref()().Get(), nullptr);
  ASSERT_NE(obj.Ref()({lhs, rhs}).Get(), nullptr);
  ASSERT_NE(obj.Ref()(lhs, rhs).Get(), nullptr);
  ASSERT_NE(obj.Ref()[2].Get(), nullptr);

  ASSERT_NE(Arg(lhs).Resolve(ctx), nullptr);
  ASSERT_NE(Arg(lhs.Ref()).Resolve(ctx), nullptr);
  ASSERT_NE(Arg(expr_attr).Resolve(ctx), nullptr);
  ASSERT_NE(Arg(lhs.Get()).Resolve(ctx), nullptr);
  ASSERT_NE(ident_from_cstr.Resolve(ctx), nullptr);
  ASSERT_NE(ident_from_string.Resolve(ctx), nullptr);
  ASSERT_NE(int_arg.Resolve(ctx), nullptr);
  ASSERT_NE(bool_arg.Resolve(ctx), nullptr);
  ASSERT_NE(null_arg.Resolve(ctx), nullptr);
  ASSERT_NE(string_lit.Resolve(ctx), nullptr);
  ASSERT_NE(Arg({lhs, rhs, nullptr}).Resolve(ctx), nullptr);
  ASSERT_NE(Arg(std::vector<Arg>{lhs, rhs, nullptr}).Resolve(ctx), nullptr);

  auto lambda_capture = ast.CaptureRef(lhs);
  EXPECT_EQ(lambda_capture.name, "lhs");
  EXPECT_EQ(lambda_capture.kind, LambdaCaptureSpec::Kind::kByRef);

  auto unary_not = !lhs.Ref();
  auto unary_neg = -lhs.Ref();
  auto unary_bit_not = ~lhs.Ref();
  auto eq_expr = lhs.Ref() == rhs;
  auto ne_expr = lhs.Ref() != 1;
  auto lt_expr = lhs.Ref() < rhs;
  auto le_expr = lhs.Ref() <= rhs;
  auto gt_expr = lhs.Ref() > rhs;
  auto ge_expr = lhs.Ref() >= rhs;
  auto land_expr = lhs.Ref() && rhs;
  auto lor_expr = lhs.Ref() || rhs;
  auto add_expr = lhs.Ref() + rhs;
  auto sub_expr = lhs.Ref() - rhs;
  auto mul_expr = lhs.Ref() * rhs;
  auto div_expr = lhs.Ref() / rhs;
  auto mod_expr = lhs.Ref() % rhs;
  auto band_expr = lhs.Ref() & rhs;
  auto bor_expr = lhs.Ref() | rhs;
  auto bxor_expr = lhs.Ref() ^ rhs;
  auto shl_expr = lhs.Ref() << 2;
  auto shr_expr = lhs.Ref() >> 1;
  auto assign_expr = ast.Assign(lhs, rhs);
  auto lambda_expr =
      ast.Lambda({LambdaCaptureSpec{"rhs", LambdaCaptureSpec::Kind::kByValue}, lambda_capture}, {ast.Return(lhs)});
  auto call_expr = ast.Call("Compute", {lhs, rhs, string_lit});
  auto reinterpret_expr = ast.ReinterpretCast("void *", ptr);

  std::vector<Stmt *> body_vec = {
      ast.VarDecl("auto", "name", string_lit),
      ast.VarDecl(lhs, 1),
      ast.Return(),
  };
  auto resolved_body = ast.Body(std::vector<BodyItem>{BodyItem(ast.Call("Touch", {lhs})), BodyItem(ast.Return(lhs))});
  ASSERT_EQ(resolved_body.size(), 2U);

  auto *decl_fn = ast.DeclareFunction("DeclVec", std::vector<VarRef>{lhs, rhs}, "uint32_t");
  auto *decl_fn_init = ast.DeclareFunction("DeclInit", {lhs}, "void");
  auto *decl_method = ast.DeclareMethod("MethodVec", std::vector<VarRef>{lhs}, "void");
  auto *decl_method_init = ast.DeclareMethod("MethodInit", {lhs, rhs}, "void");
  auto *def_fn_vec = ast.DefineFunction("DefVec", std::vector<VarRef>{lhs, rhs}, "uint32_t", body_vec);
  auto *def_fn_init = ast.DefineFunction("DefInit", {lhs}, "uint32_t", {
      ast.VarDecl("uint32_t", "tmp", add_expr),
      ast.Return(lhs),
  });
  auto *def_method_vec =
      ast.DefineMethod("Worker", "ExecVec", std::vector<VarRef>{lhs}, "void", std::vector<Stmt *>{ExprStmt::Create(ctx, call_expr.Get())});
  auto *def_method_init = ast.DefineMethod("Worker", "ExecInit", {lhs}, "void", {
      ast.Return(),
  });
  auto *all_ops_def = ast.DefineFunction("AllOps", {lhs, rhs, ptr, obj, arr, runner}, "uint32_t", {
      CommentStmt::Create(ctx, "dsl-all-ops"),
      BlankLineStmt::Create(ctx),
      ast.VarDecl("auto", "from_cstr", ident_from_cstr),
      ast.VarDecl("auto", "from_string", ident_from_string),
      ast.VarDecl("auto", "str_node", ast.Str("dsl")),
      ast.VarDecl("auto", "uint_node", ast.UInt(9)),
      ast.VarDecl("auto", "ulong_node", ast.ULong(9)),
      ast.VarDecl("auto", "eq_v", eq_expr),
      ast.VarDecl("auto", "ne_v", ne_expr),
      ast.VarDecl("auto", "lt_v", lt_expr),
      ast.VarDecl("auto", "le_v", le_expr),
      ast.VarDecl("auto", "gt_v", gt_expr),
      ast.VarDecl("auto", "ge_v", ge_expr),
      ast.VarDecl("auto", "land_v", land_expr),
      ast.VarDecl("auto", "lor_v", lor_expr),
      ast.VarDecl("auto", "add_v", add_expr),
      ast.VarDecl("auto", "sub_v", sub_expr),
      ast.VarDecl("auto", "mul_v", mul_expr),
      ast.VarDecl("auto", "div_v", div_expr),
      ast.VarDecl("auto", "mod_v", mod_expr),
      ast.VarDecl("auto", "band_v", band_expr),
      ast.VarDecl("auto", "bor_v", bor_expr),
      ast.VarDecl("auto", "bxor_v", bxor_expr),
      ast.VarDecl("auto", "shl_v", shl_expr),
      ast.VarDecl("auto", "shr_v", shr_expr),
      ast.VarDecl("auto", "not_v", unary_not),
      ast.VarDecl("auto", "neg_v", unary_neg),
      ast.VarDecl("auto", "bit_not_v", unary_bit_not),
      ast.VarDecl("auto", "index_v", arr[2]),
      ast.VarDecl("auto", "addr_v", lhs.Addr()),
      ast.VarDecl("auto", "attr_v", obj.Attr("field")),
      ast.VarDecl("auto", "arrow_v", ptr.Arrow("field")),
      ast.VarDecl("auto", "call0_v", runner()),
      ast.VarDecl("auto", "call1_v", runner(lhs)),
      ast.VarDecl("auto", "member_call_v", obj.Attr("Exec")(lhs, rhs)),
      ast.VarDecl("auto", "lambda_v", lambda_expr),
      ast.VarDecl("auto", "reinterpret_v", reinterpret_expr),
      ast.VarDecl("auto", "init_v", ast.InitList(std::vector<Arg>{lhs, rhs, null_arg})),
      ast.Assign(obj.Attr("field"), call_expr),
      IfStmt::Create(ctx, gt_expr.Get(), BlockStmt::Create(ctx, ast.Body({ast.Return(lhs)})),
                     BlockStmt::Create(ctx, ast.Body({ast.Return(rhs)}))),
      ast.Return(lhs),
  });
  auto *dsl_class =
      ast.Class("DslClass", std::vector<DeclNode *>{ast.Public(), ast.Field("uint32_t", "field", ast.UInt(4)),
                                                    decl_method, decl_method_init, ast.Private(),
                                                    ast.Field("uint32_t", "hidden")});
  auto *dsl_struct = ast.Struct("DslStruct", {ast.Field("const char *", "name", ast.Str("abc"))});
  std::vector<DeclNode *> namespace_items{ast.TypeAlias("void *", "DslHandle"), dsl_class, dsl_struct, decl_fn_init,
                                          def_fn_vec, def_fn_init, def_method_vec, def_method_init, all_ops_def};
  auto *dsl_namespace = ast.Namespace("dsl_ns", namespace_items);
  std::vector<DeclNode *> tu_items{ast.Include("vector", IncludeDecl::Kind::kAngle), ast.Include("local.h"),
                                   ast.Space(),
                                   ast.StablePart(StablePartId::kScopeGuard), ast.ExternBlock("C", {decl_fn}),
                                   dsl_namespace};
  auto *tu = ast.File(tu_items);
  ASSERT_NE(tu, nullptr);

  EXPECT_EQ(decl_fn->GetParams().Size(), 2U);
  EXPECT_EQ(decl_fn_init->GetParams().Size(), 1U);
  EXPECT_EQ(decl_method->GetParams().Size(), 1U);
  EXPECT_EQ(def_fn_vec->GetBody()->GetStatements().Size(), 3U);
  EXPECT_EQ(def_fn_init->GetBody()->GetStatements().Size(), 2U);
  EXPECT_EQ(def_method_vec->GetBody()->GetStatements().Size(), 1U);

  const auto output = EmitNode(*tu);
  ExpectContainsAll(output, {
      "#include <vector>\n#include \"local.h\"\n\n",
      "class ScopeGuard {\n",
      "extern \"C\" {\n",
      "uint32_t DeclVec(uint32_t lhs, uint32_t rhs);\n",
      "namespace dsl_ns {\n",
      "typedef void *DslHandle;\n",
      "class DslClass {\n",
      "  public:\n",
      "    uint32_t field = 4U;\n",
      "    void MethodVec(uint32_t lhs);\n",
      "    void MethodInit(uint32_t lhs, uint32_t rhs);\n",
      "  private:\n",
      "    uint32_t hidden;\n",
      "struct DslStruct {\n",
      "const char *name = \"abc\";\n",
      "void DeclInit(uint32_t lhs);\n",
      "uint32_t DefVec(uint32_t lhs, uint32_t rhs) {\n",
      "return;\n",
      "uint32_t DefInit(uint32_t lhs) {\n",
      "void Worker::ExecVec(uint32_t lhs) {\n",
      "void Worker::ExecInit(uint32_t lhs) {\n",
      "// dsl-all-ops\n",
      "auto from_cstr = symbol_from_cstr;\n",
      "auto from_string = symbol_from_string;\n",
      "auto str_node = \"dsl\";\n",
      "auto uint_node = 9U;\n",
      "auto ulong_node = 9UL;\n",
      "auto eq_v = (lhs == rhs);\n",
      "auto ne_v = (lhs != 1);\n",
      "auto lt_v = (lhs < rhs);\n",
      "auto le_v = (lhs <= rhs);\n",
      "auto gt_v = (lhs > rhs);\n",
      "auto ge_v = (lhs >= rhs);\n",
      "auto land_v = (lhs && rhs);\n",
      "auto lor_v = (lhs || rhs);\n",
      "auto add_v = (lhs + rhs);\n",
      "auto sub_v = (lhs - rhs);\n",
      "auto mul_v = (lhs * rhs);\n",
      "auto div_v = (lhs / rhs);\n",
      "auto mod_v = (lhs % rhs);\n",
      "auto band_v = (lhs & rhs);\n",
      "auto bor_v = (lhs | rhs);\n",
      "auto bxor_v = (lhs ^ rhs);\n",
      "auto shl_v = (lhs << 2);\n",
      "auto shr_v = (lhs >> 1);\n",
      "auto not_v = !lhs;\n",
      "auto neg_v = -lhs;\n",
      "auto bit_not_v = ~lhs;\n",
      "auto index_v = arr[2];\n",
      "auto addr_v = &lhs;\n",
      "auto attr_v = obj.field;\n",
      "auto arrow_v = ptr->field;\n",
      "auto call0_v = runner();\n",
      "auto call1_v = runner(lhs);\n",
      "auto member_call_v = obj.Exec(lhs, rhs);\n",
      "auto lambda_v = [rhs, &lhs]() {\n",
      "auto reinterpret_v = reinterpret_cast<void *>(ptr);\n",
      "auto init_v = {lhs, rhs, nullptr};\n",
      "obj.field = Compute(lhs, rhs, \"string_literal\");\n",
      "if ((lhs > rhs)) {\n",
      "return lhs;\n",
      "return rhs;\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_ArgSupportsAllIntegralTypes_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  const uint32_t u32_value = 7U;
  const size_t size_value = 9U;
  const uint8_t u8_value = 3U;

  ASSERT_NE(Arg(u32_value).Resolve(ctx), nullptr);
  ASSERT_NE(Arg(size_value).Resolve(ctx), nullptr);
  ASSERT_NE(Arg(u8_value).Resolve(ctx), nullptr);

  auto *fn = ast.DefineFunction("IntegralArgs", {}, "void", {
      ast.VarDecl("auto", "u32_v", u32_value),
      ast.VarDecl("auto", "size_v", size_value),
      ast.VarDecl("auto", "u8_v", u8_value),
      ast.Return(),
  });
  ASSERT_NE(fn, nullptr);

  const auto output = EmitNode(*fn);
  ExpectContainsAll(output, {
      "auto u32_v = 7;\n",
      "auto size_v = 9;\n",
      "auto u8_v = 3;\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_MakeUniqueArray_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto count = ast.Var("size_t", "count");
  auto *fn = ast.DefineFunction("BuildArrays", {count}, "void", {
      ast.VarDecl("auto", "builtin_buffer", ast.MakeUniqueArray(BuiltinType::kUInt8, count)),
      ast.VarDecl("auto", "custom_buffer", ast.MakeUniqueArray("CustomType", 4)),
      ast.Return(),
  });
  ASSERT_NE(fn, nullptr);

  const auto output = EmitNode(*fn);
  ExpectContainsAll(output, {
      "auto builtin_buffer = std::make_unique<uint8_t[]>(count);\n",
      "auto custom_buffer = std::make_unique<CustomType[]>(4);\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_ControlFlowAndCtorInit_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto value = ast.Var("int", "value");
  auto values = ast.Var("std::vector<int>", "values");
  auto i = ast.Var("size_t", "i");
  auto lhs = ast.Var("int", "lhs");
  auto item = ast.Var("auto", "item");

  auto *for_stmt = ast.For(ast.VarDecl(i, 0), i < 4, ast.PreInc(i), {
      ast.Assign(value, value + 1),
  });
  auto *range_for_stmt = ast.RangeFor(item, values, {
      ast.Assign(value, value + item),
  });
  auto *ctor_def = ast.DefineMethod("Worker", "Worker", {lhs}, "", {ast.MemberInit("value_", lhs)}, {
      ast.Assign(value, lhs),
  });
  auto *tu = ast.File({
      ast.Namespace("om2", {
          ctor_def,
          ast.DefineFunction("Touch", {value, values}, "void", {
              for_stmt,
              range_for_stmt,
              ast.Return(),
          }),
      }),
  });

  ASSERT_NE(tu, nullptr);
  const auto output = EmitNode(*tu);
  ExpectContainsAll(output, {
      "Worker::Worker(int lhs)\n",
      "  : value_(lhs) {\n",
      "for (size_t i = 0; (i < 4); ++i) {\n",
      "for (auto item : values) {\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_MemcpyAndSizeof_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto dst = ast.Var("void *", "dst");
  auto src = ast.Var("const void *", "src");
  auto addr = ast.Var("uintptr_t", "addr");
  auto *tu = ast.File({
      ast.Namespace("om2", {
          ast.DefineFunction("TouchMemcpy", {dst, src, addr}, "void", {
              BodyItem(ast.Memcpy(dst, src, ast.Sizeof(addr))),
              ast.Return(),
          }),
      }),
  });

  ASSERT_NE(tu, nullptr);
  const auto output = EmitNode(*tu);
  ExpectContainsAll(output, {
      "void TouchMemcpy(void *dst, const void *src, uintptr_t addr) {\n",
      "std::memcpy(dst, src, sizeof(addr));\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_IgnoreOutputRemoveFile_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto json_path = ast.Var("std::string", "json_path");
  auto *fn = ast.DefineFunction("CleanupJsonFile", {json_path}, "void", {
      ast.IgnoreOutput(ast.RemoveFile(json_path.CStr())),
      ast.Return(),
  });
  ASSERT_NE(fn, nullptr);

  const auto output = EmitNode(*fn);
  ExpectContainsAll(output, {
      "void CleanupJsonFile(std::string json_path) {\n",
      "(void)std::remove(json_path.c_str());\n",
  });
}

TEST_F(Om2CodegenUt, InterfaceDumpApis_EmitInCLinkageAndPtrToU64Outside_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto *tu = ast.File({
      ast.StablePart(StablePartId::kInterfacePointerHelpers),
      ast.ExternBlock("C", {ast.StablePart(StablePartId::kInterfaceDumpApis)}),
  });
  ASSERT_NE(tu, nullptr);

  const auto output = EmitNode(*tu);
  ExpectContainsAll(output, {
      "inline void *ValueToPtr(const uint64_t value) {\n",
      "inline uint64_t PtrToU64(const void *ptr) {\n",
      "extern \"C\" {\n",
      "struct Om2Tensor {\n",
      "__attribute__((weak)) int32_t ReportTaskInfo(uint32_t model_id,\n",
  });
  EXPECT_LT(output.find("inline uint64_t PtrToU64"), output.find("extern \"C\" {"));
  EXPECT_GT(output.find("struct Om2Tensor"), output.find("extern \"C\" {"));
}

TEST_F(Om2CodegenUt, LoadAndRunDumpHelpers_EmitInAnonymousNamespace_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto *tu = ast.File({
      ast.Namespace("om2", {
          ast.Namespace("", {ast.StablePart(StablePartId::kLoadAndRunDumpHelpers)}),
      }),
  });
  ASSERT_NE(tu, nullptr);

  const auto output = EmitNode(*tu);
  ExpectContainsAll(output, {
      "namespace om2 {\n",
      "namespace {\n",
      "Om2Tensor BuildOm2Tensor(void *device_address, uint64_t size, int32_t data_type,\n",
      "aclError ReportLaunchedOm2Task(const char *op_name, const char *op_type, uint64_t op_desc_id,\n",
      "const std::vector<uint64_t> &workspace_addrs,\n",
      "OM2_CHK_TRUE(workspace_addrs.size() == workspace_sizes.size());\n",
      "uint8_t GetIsDataDump(const char *op_name, uint32_t model_id, void *instance_handle) {\n",
  });
}

TEST_F(Om2CodegenUt, AstDsl_ContainerMethods_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto vec = ast.Var("std::vector<uint8_t>", "vec");
  auto ptr = ast.Var("std::unique_ptr<uint8_t[]>", "ptr");
  auto index = ast.Var("size_t", "index");
  auto value = ast.Var("uint8_t", "value");
  auto *fn = ast.DefineFunction("TouchContainer", {vec, ptr, index, value}, "void", {
      vec.Clear(),
      vec.Resize(8),
      vec.PushBack(value),
      ast.Call("Use", {vec.Size(), vec.Data(), vec.Empty(), vec.At(index), ptr.GetPtr()}),
      ast.Return(),
  });
  ASSERT_NE(fn, nullptr);

  const auto output = EmitNode(*fn);
  ExpectContainsAll(output, {
      "void TouchContainer(std::vector<uint8_t> vec, std::unique_ptr<uint8_t[]> ptr, size_t index, uint8_t value) {\n",
      "vec.clear();\n",
      "vec.resize(8);\n",
      "vec.push_back(value);\n",
      "Use(vec.size(), vec.data(), vec.empty(), vec.at(index), ptr.get());\n",
  });
}

TEST_F(Om2CodegenUt, Arg_AutoPromoteInitList_Ok) {
  AstContext ctx;
  AstBuildContext ast(ctx);

  auto value = ast.Var("std::vector<ArgsInfo>", "value");
  auto *fn = ast.DefineFunction("Build", {}, "void", {
      ast.Assign(value, {{1, 2, 3}, {4, 5, 6}}),
  });

  ASSERT_NE(fn, nullptr);
  const auto output = EmitNode(*fn);
  ExpectContainsAll(output, {
      "void Build() {\n",
      "value = {{1, 2, 3}, {4, 5, 6}};\n",
  });
}

TEST_F(Om2CodegenUt, CompileGeneratedCppToSo_MakefileVariableContinuation_Ok) {
  ScopedEnvVar asan_guard("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=0");
  ScopedEnvVar lsan_guard("LSAN_OPTIONS", "exitcode=0");
  const std::string model_name = "continuation_test";
  const std::string interface_name = model_name + "_interface.h";
  const std::string include_line = "#include \"" + interface_name + "\"\n";
  Om2CodegenArtifacts artifacts = {
      {interface_name, "#pragma once\n#define CONTINUATION_TEST_VALUE 7\n"},
      {model_name + "_resources.cpp",
       include_line + "extern \"C\" int ContinuationTestResources() { return CONTINUATION_TEST_VALUE; }\n"},
      {model_name + "_kernel_reg.cpp",
       include_line + "extern \"C\" int ContinuationTestKernelReg() { return CONTINUATION_TEST_VALUE; }\n"},
      {model_name + "_load_and_run.cpp",
       include_line + "extern \"C\" int ContinuationTestLoadAndRun() { return CONTINUATION_TEST_VALUE; }\n"},
      {model_name + "_args_manager.cpp",
       include_line + "extern \"C\" int ContinuationTestArgsManager() { return CONTINUATION_TEST_VALUE; }\n"},
      {"Makefile", R"(CXX := g++
TARGET := libcontinuation_test_om2.so
SRC_FILES := continuation_test_resources.cpp \
  \
  continuation_test_kernel_reg.cpp \
  continuation_test_load_and_run.cpp \
  continuation_test_args_manager.cpp

CXXFLAGS := -std=c++17 -fPIC
LDFLAGS := -shared

all: $(TARGET)

$(TARGET): $(SRC_FILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
)"},
  };

  Om2CodegenArtifact so_artifact;
  ScopedStdoutCapture stdout_capture;
  ASSERT_EQ(Om2Utils::CompileGeneratedCppToSo(artifacts, model_name, so_artifact, false), SUCCESS);
  const std::string compile_stdout = stdout_capture.Stop();
  EXPECT_EQ(so_artifact.file_name, "lib" + model_name + "_om2.so");
  EXPECT_FALSE(so_artifact.data.empty());
  EXPECT_EQ(compile_stdout.find("g++ -std=c++17 -fPIC"), std::string::npos);
}

TEST_F(Om2CodegenUt, StablePartProvider_AllIds_Ok) {
  const std::vector<std::pair<StablePartId, std::string>> cases = {
      {StablePartId::kChkStatusMacro, "#define OM2_CHK_STATUS"},
      {StablePartId::kChkNotNullMacro, "#define OM2_CHK_NOTNULL"},
      {StablePartId::kChkTrueMacro, "#define OM2_CHK_TRUE"},
      {StablePartId::kGetAddrMacro, "#define GET_ADDR"},
      {StablePartId::kMakeGuardMacro, "#define OM2_MAKE_GUARD"},
      {StablePartId::kInterfaceMacros, "#define OM2_CHK_STATUS"},
      {StablePartId::kPointerHelpers, "inline uint64_t PtrToValue"},
      {StablePartId::kFlattenHostArgs, "inline std::vector<uint64_t> FlattenHostArgs"},
      {StablePartId::kInterfacePointerHelpers, "inline std::vector<uint64_t> FlattenHostArgs"},
      {StablePartId::kScopeGuard, "class ScopeGuard"},
      {StablePartId::kReadBinaryFileToBuffer, "BinaryBuffer ReadBinaryFileToBuffer"},
      {StablePartId::kGenerateJsonFile, "aclError GenerateJsonFile"},
      {StablePartId::kInterfaceDumpApis, "struct Om2TaskInfo"},
      {StablePartId::kLoadAndRunDumpHelpers, "aclError ReportLaunchedOm2Task"},
  };

  for (const auto &test_case : cases) {
    std::string output;
    ASSERT_EQ(ResolveStablePart(test_case.first, output), SUCCESS);
    EXPECT_NE(output.find(test_case.second), std::string::npos) << output;
  }

  std::string output;
  ASSERT_EQ(ResolveStablePart(static_cast<StablePartId>(0xff), output), FAILED);
  EXPECT_TRUE(output.empty());
}
}  // namespace ge
