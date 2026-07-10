/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_OM2_CODEGEN_AST_AST_BUILD_CONTEXT_H
#define BASE_COMMON_OM2_CODEGEN_AST_AST_BUILD_CONTEXT_H

#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "ast_context.h"
#include "ast_nodes.h"

namespace ge {
struct LambdaCaptureSpec {
  enum class Kind : uint8_t {
    kByValue = 0,
    kByRef,
  };

  std::string name;
  Kind kind;
};

class ExprRef;
class VarRef;
class BodyItem;

class Arg {
 public:
  enum class Kind : uint8_t {
    kEmpty = 0,
    kExpr,
    kIdentifier,
    kStringLiteral,
    kInt,
    kBool,
    kNullptr,
    kInitList,
  };

  Arg();
  Arg(const char *text);
  Arg(const std::string &text);
  Arg(bool value);
  Arg(std::nullptr_t);
  Arg(std::initializer_list<Arg> items);
  Arg(const std::vector<Arg> &items);
  Arg(Expr *expr);
  Arg(const ExprRef &expr);
  Arg(const VarRef &symbol);

  template <typename T, typename = std::enable_if_t<std::is_integral<typename std::decay<T>::type>::value &&
                                                    !std::is_same<typename std::decay<T>::type, bool>::value>>
  Arg(T value) : kind_(Kind::kInt), int_value_(static_cast<int64_t>(value)), bool_value_(false), expr_(nullptr) {}

  static Arg StringLiteral(const std::string &text);

  bool Empty() const;
  Expr *Resolve(AstContext &ctx) const;

 private:
  Kind kind_;
  std::string text_;
  int64_t int_value_;
  bool bool_value_;
  Expr *expr_;
  std::vector<Arg> init_list_items_;
};

struct MemberInitSpec {
  std::string member_name;
  Arg init;
};

class ExprRef {
 public:
  ExprRef(AstContext &ctx, Expr *expr);

  Expr *Get() const;
  AstContext *Ctx() const;
  ExprRef Attr(const std::string &field) const;
  ExprRef Arrow(const std::string &field) const;
  ExprRef Addr() const;
  ExprRef operator()() const;
  ExprRef operator()(std::initializer_list<Arg> args) const;
  template <typename... ArgsT>
  ExprRef operator()(ArgsT &&...args) const {
    return (*this)({Arg(std::forward<ArgsT>(args))...});
  }
  ExprRef operator[](Arg index) const;
  ExprRef Clear() const;
  ExprRef Resize(Arg size) const;
  ExprRef Size() const;
  ExprRef Data() const;
  ExprRef Empty() const;
  ExprRef At(Arg index) const;
  ExprRef PushBack(Arg value) const;
  ExprRef CStr() const;
  ExprRef GetPtr() const;

 private:
  AstContext *ctx_;
  Expr *expr_;
};

class VarRef : public ExprRef {
 public:
  VarRef(AstContext &ctx, std::string type_name, const std::string &symbol_name);

  const std::string &TypeName() const;
  const std::string &SymbolName() const;
  ExprRef Ref() const {
    return ExprRef(*Ctx(), Get());
  }

 private:
  std::string type_name_;
  std::string symbol_name_;
};

ExprRef operator!(ExprRef expr);
ExprRef operator-(ExprRef expr);
ExprRef operator~(ExprRef expr);
ExprRef operator==(ExprRef lhs, Arg rhs);
ExprRef operator!=(ExprRef lhs, Arg rhs);
ExprRef operator<(ExprRef lhs, Arg rhs);
ExprRef operator<=(ExprRef lhs, Arg rhs);
ExprRef operator>(ExprRef lhs, Arg rhs);
ExprRef operator>=(ExprRef lhs, Arg rhs);
ExprRef operator&&(ExprRef lhs, Arg rhs);
ExprRef operator||(ExprRef lhs, Arg rhs);
ExprRef operator+(ExprRef lhs, Arg rhs);
ExprRef operator-(ExprRef lhs, Arg rhs);
ExprRef operator*(ExprRef lhs, Arg rhs);
ExprRef operator/(ExprRef lhs, Arg rhs);
ExprRef operator%(ExprRef lhs, Arg rhs);
ExprRef operator&(ExprRef lhs, Arg rhs);
ExprRef operator|(ExprRef lhs, Arg rhs);
ExprRef operator^(ExprRef lhs, Arg rhs);
ExprRef operator<<(ExprRef lhs, Arg rhs);
ExprRef operator>>(ExprRef lhs, Arg rhs);

class AstBuildContext {
 public:
  explicit AstBuildContext(AstContext &ctx);

  AstBuildContext(const AstBuildContext &) = delete;
  AstBuildContext &operator=(const AstBuildContext &) = delete;
  AstBuildContext(AstBuildContext &&) = delete;
  AstBuildContext &operator=(AstBuildContext &&) = delete;

  TranslationUnit *File(const std::vector<DeclNode *> &items) const;
  TranslationUnit *File(std::initializer_list<DeclNode *> items) const;
  IncludeDecl *Include(const std::string &path, IncludeDecl::Kind kind = IncludeDecl::Kind::kQuote) const;
  SpaceDecl *Space() const;
  CommentStmt *Comment(const std::string &text) const;
  BlankLineStmt *BlankLine() const;
  AccessSectionDecl *Public() const;
  AccessSectionDecl *Private() const;
  Expr *Str(const std::string &text) const;
  Expr *UInt(uint64_t value) const;
  Expr *ULong(uint64_t value) const;
  TypeAliasDecl *TypeAlias(const std::string &type_spec, const std::string &name) const;
  FieldDecl *Field(const std::string &type_spec, const std::string &name, Arg init = {}) const;
  ClassDecl *Class(const std::string &name, const std::vector<DeclNode *> &items) const;
  ClassDecl *Class(const std::string &name, std::initializer_list<DeclNode *> items) const;
  StructDecl *Struct(const std::string &name, const std::vector<DeclNode *> &items) const;
  StructDecl *Struct(const std::string &name, std::initializer_list<DeclNode *> items) const;
  NamespaceDecl *Namespace(const std::string &name, const std::vector<DeclNode *> &items) const;
  NamespaceDecl *Namespace(const std::string &name, std::initializer_list<DeclNode *> items) const;
  ExternBlockDecl *ExternBlock(const std::string &lang, const std::vector<DeclNode *> &items) const;
  ExternBlockDecl *ExternBlock(const std::string &lang, std::initializer_list<DeclNode *> items) const;
  StablePartDecl *StablePart(StablePartId id,
                             StablePartPlacement placement = StablePartPlacement::kTranslationUnit) const;
  FunctionDecl *DeclareFunction(const std::string &name, const std::vector<VarRef> &params,
                                const std::string &return_type) const;
  FunctionDecl *DeclareFunction(const std::string &name, std::initializer_list<VarRef> params,
                                const std::string &return_type) const;
  MethodDecl *DeclareMethod(const std::string &name, const std::vector<VarRef> &params, const std::string &return_type,
                            const std::string &trailing_spec = "") const;
  MethodDecl *DeclareMethod(const std::string &name, std::initializer_list<VarRef> params,
                            const std::string &return_type, const std::string &trailing_spec = "") const;
  FunctionDef *DefineFunction(const std::string &name, const std::vector<VarRef> &params,
                              const std::string &return_type, const std::vector<Stmt *> &body) const;
  FunctionDef *DefineFunction(const std::string &name, std::initializer_list<VarRef> params,
                              const std::string &return_type, std::initializer_list<BodyItem> items) const;
  MethodDef *DefineMethod(const std::string &owner, const std::string &name, const std::vector<VarRef> &params,
                          const std::string &return_type, const std::vector<Stmt *> &body) const;
  MethodDef *DefineMethod(const std::string &owner, const std::string &name, const std::vector<VarRef> &params,
                          const std::string &return_type, const std::vector<BodyItem> &items) const;
  MethodDef *DefineMethod(const std::string &owner, const std::string &name, std::initializer_list<VarRef> params,
                          const std::string &return_type, std::initializer_list<BodyItem> items) const;
  MethodDef *DefineMethod(const std::string &owner, const std::string &name, const std::vector<VarRef> &params,
                          const std::string &return_type, const std::vector<MemberInitSpec> &member_inits,
                          const std::vector<Stmt *> &body) const;
  MethodDef *DefineMethod(const std::string &owner, const std::string &name, const std::vector<VarRef> &params,
                          const std::string &return_type, const std::vector<MemberInitSpec> &member_inits,
                          const std::vector<BodyItem> &items) const;
  MethodDef *DefineMethod(const std::string &owner, const std::string &name, std::initializer_list<VarRef> params,
                          const std::string &return_type, std::initializer_list<MemberInitSpec> member_inits,
                          std::initializer_list<BodyItem> items) const;
  CCastExpr *CCast(const std::string &target_type, Arg expr) const;
  InitListExpr *InitList(std::initializer_list<Arg> items, bool compact = false) const;
  InitListExpr *InitList(const std::vector<Arg> &items, bool compact = false) const;
  DesignatedInitListExpr *DesignatedInit(const std::vector<std::pair<std::string, Arg>> &members,
                                         bool compact = false) const;
  VarRef Var(const std::string &type_name, const std::string &symbol_name) const;
  ExprRef Assign(Arg lhs, Arg rhs) const;
  ExprRef Deref(Arg expr) const;
  ExprRef PreInc(Arg expr) const;
  ExprRef PostInc(Arg expr) const;
  ExprRef ToStr(Arg expr) const;
  ExprRef Memcpy(Arg dst, Arg src, Arg size) const;
  ExprRef Sizeof(Arg expr) const;
  ExprRef RemoveFile(Arg path) const;
  ExprRef IgnoreOutput(Arg expr) const;
  LambdaCaptureSpec CaptureRef(const VarRef &symbol) const;
  ExprRef Lambda(std::initializer_list<LambdaCaptureSpec> captures, std::initializer_list<BodyItem> items) const;
  ExprRef Call(const std::string &callee_name, std::initializer_list<Arg> args) const;
  ExprRef Call(const std::string &callee_name, const std::vector<Arg> &args) const;
  ExprRef MakeUniqueArray(BuiltinType elem_type, Arg count) const;
  ExprRef MakeUniqueArray(const std::string &elem_type, Arg count) const;
  ExprRef MakeUniqueArray(const char *elem_type, Arg count) const;
  ReturnStmt *Return(Arg value = {}) const;
  VarDeclStmt *VarDecl(const std::string &type_spec, const std::string &name, Arg init = {}) const;
  VarDeclStmt *VarDecl(const VarRef &symbol, Arg init = {}) const;
  BlockStmt *Block(const std::vector<BodyItem> &items) const;
  IfStmt *If(Arg cond, std::initializer_list<BodyItem> then_items) const;
  IfStmt *If(Arg cond, std::initializer_list<BodyItem> then_items, std::initializer_list<BodyItem> else_items,
             bool is_preprocessor = false) const;
  IfStmt *If(Arg cond, const std::vector<BodyItem> &then_items, const std::vector<BodyItem> &else_items,
             bool is_preprocessor = false) const;
  ForStmt *For(Stmt *init, Arg cond, Arg step, std::initializer_list<BodyItem> items) const;
  RangeForStmt *RangeFor(const VarRef &loop_var, Arg range, std::initializer_list<BodyItem> items) const;
  RangeForStmt *RangeFor(const std::string &type_spec, const std::string &name, Arg range,
                         std::initializer_list<BodyItem> items) const;
  SwitchStmt *Switch(Arg cond, const std::vector<BodyItem> &items) const;
  CaseStmt *Case(Arg value) const;
  BreakStmt *Break() const;
  MemberInitSpec MemberInit(const std::string &member_name, Arg init) const;
  std::vector<Stmt *> Body(std::initializer_list<BodyItem> items) const;
  std::vector<Stmt *> Body(const std::vector<BodyItem> &items) const;
  ExprRef StaticCast(const std::string &target_type, Arg expr) const;
  ExprRef ConstCast(const std::string &target_type, Arg expr) const;
  ExprRef ReinterpretCast(const std::string &target_type, Arg expr) const;

 private:
  AstContext &ctx_;

  ExprRef MakeUniqueArray(const TypeName &elem_type, Arg count) const;
};

class BodyItem {
 public:
  BodyItem(Stmt *stmt);
  BodyItem(Expr *expr);
  BodyItem(const ExprRef &expr);
  BodyItem(const VarRef &symbol);

  Stmt *Resolve(AstContext &ctx) const;

 private:
  Stmt *stmt_;
  Expr *expr_;
};
}  // namespace ge

#endif  // BASE_COMMON_OM2_CODEGEN_AST_AST_BUILD_CONTEXT_H
