/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ast_nodes.h"

#include <utility>

namespace ge {
namespace {
template <typename T>
auto CopyArray(AstContext &ctx, const std::vector<T> &items) -> ArrayRef<T> {
  auto copied = ctx.AllocateMutableArray<T>(items.size());
  GE_ASSERT_TRUE(copied.Data() != nullptr || items.empty());
  for (size_t i = 0; i < items.size(); ++i) {
    copied[i] = items[i];
  }
  return ArrayRef<T>(copied.Data(), copied.Size());
}

template <typename NodeT, typename... Args>
auto AllocateNode(AstContext &ctx, Args &&...args) -> NodeT * {
  void *mem = ctx.Allocate(sizeof(NodeT));
  GE_ASSERT_NOTNULL(mem);
  return new (mem) NodeT(std::forward<Args>(args)...);
}

template <typename NodeT>
auto CreateNamedContainer(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items) -> NodeT * {
  return AllocateNode<NodeT>(ctx, ctx.CopyString(name.c_str()), CopyArray(ctx, items));
}
}  // namespace

ParamDecl *ParamDecl::Create(AstContext &ctx, const std::string &type_spec, const std::string &name) {
  return AllocateNode<ParamDecl>(ctx, ctx.CopyString(type_spec.c_str()), ctx.CopyString(name.c_str()));
}

Status ParamDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

TranslationUnit *TranslationUnit::Create(AstContext &ctx, const std::vector<DeclNode *> &items) {
  return AllocateNode<TranslationUnit>(ctx, CopyArray(ctx, items));
}

Status TranslationUnit::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

StablePartDecl *StablePartDecl::Create(AstContext &ctx, StablePartId id, StablePartRole role,
                                       StablePartPlacement placement) {
  return AllocateNode<StablePartDecl>(ctx, id, role, placement);
}

Status StablePartDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

IncludeDecl *IncludeDecl::Create(AstContext &ctx, const std::string &path, Kind kind) {
  return AllocateNode<IncludeDecl>(ctx, ctx.CopyString(path.c_str()), kind);
}

Status IncludeDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

SpaceDecl *SpaceDecl::Create(AstContext &ctx) {
  return AllocateNode<SpaceDecl>(ctx);
}

Status SpaceDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

TypeAliasDecl *TypeAliasDecl::Create(AstContext &ctx, const std::string &type_spec, const std::string &name) {
  return AllocateNode<TypeAliasDecl>(ctx, ctx.CopyString(type_spec.c_str()), ctx.CopyString(name.c_str()));
}

Status TypeAliasDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

NamespaceDecl *NamespaceDecl::Create(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items) {
  return CreateNamedContainer<NamespaceDecl>(ctx, name, items);
}

Status NamespaceDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ExternBlockDecl *ExternBlockDecl::Create(AstContext &ctx, const std::string &language,
                                         const std::vector<DeclNode *> &items) {
  return CreateNamedContainer<ExternBlockDecl>(ctx, language, items);
}

Status ExternBlockDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ClassDecl *ClassDecl::Create(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items) {
  return CreateNamedContainer<ClassDecl>(ctx, name, items);
}

Status ClassDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

StructDecl *StructDecl::Create(AstContext &ctx, const std::string &name, const std::vector<DeclNode *> &items) {
  return CreateNamedContainer<StructDecl>(ctx, name, items);
}

Status StructDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

AccessSectionDecl *AccessSectionDecl::Create(AstContext &ctx, Kind kind) {
  return AllocateNode<AccessSectionDecl>(ctx, kind);
}

Status AccessSectionDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

FieldDecl *FieldDecl::Create(AstContext &ctx, const std::string &type_spec, const std::string &name, Expr *init) {
  return AllocateNode<FieldDecl>(ctx, ctx.CopyString(type_spec.c_str()), ctx.CopyString(name.c_str()), init);
}

Status FieldDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

MethodDecl *MethodDecl::Create(AstContext &ctx, const std::string &name, const std::vector<ParamDecl *> &params,
                               const std::string &return_type, const std::string &trailing_spec) {
  return AllocateNode<MethodDecl>(ctx, ctx.CopyString(name.c_str()), CopyArray(ctx, params),
                                  ctx.CopyString(return_type.c_str()), ctx.CopyString(trailing_spec.c_str()));
}

Status MethodDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

FunctionDecl *FunctionDecl::Create(AstContext &ctx, const std::string &name, const std::vector<ParamDecl *> &params,
                                   const std::string &return_type) {
  return AllocateNode<FunctionDecl>(ctx, ctx.CopyString(name.c_str()), CopyArray(ctx, params),
                                    ctx.CopyString(return_type.c_str()));
}

Status FunctionDecl::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

FunctionDef *FunctionDef::Create(AstContext &ctx, const std::string &name, const std::vector<ParamDecl *> &params,
                                 const std::string &return_type, BlockStmt *body) {
  GE_ASSERT_NOTNULL(body);
  return AllocateNode<FunctionDef>(ctx, ctx.CopyString(name.c_str()), CopyArray(ctx, params),
                                   ctx.CopyString(return_type.c_str()), body);
}

Status FunctionDef::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

MethodDef *MethodDef::Create(AstContext &ctx, const std::string &owner, const std::string &name,
                             const std::vector<ParamDecl *> &params, const std::string &return_type,
                             const std::vector<std::string> &member_init_names,
                             const std::vector<Expr *> &member_init_exprs, BlockStmt *body) {
  GE_ASSERT_NOTNULL(body);
  GE_ASSERT_TRUE(member_init_names.size() == member_init_exprs.size());
  std::vector<StringRef> copied_member_init_names;
  copied_member_init_names.reserve(member_init_names.size());
  for (const auto &member_init_name : member_init_names) {
    copied_member_init_names.push_back(ctx.CopyString(member_init_name.c_str()));
  }
  return AllocateNode<MethodDef>(ctx, ctx.CopyString(owner.c_str()), ctx.CopyString(name.c_str()),
                                 CopyArray(ctx, params), ctx.CopyString(return_type.c_str()),
                                 CopyArray(ctx, copied_member_init_names), CopyArray(ctx, member_init_exprs), body);
}

Status MethodDef::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

IdentifierExpr *IdentifierExpr::Create(AstContext &ctx, const std::string &name) {
  return AllocateNode<IdentifierExpr>(ctx, ctx.CopyString(name.c_str()));
}

Status IdentifierExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

LiteralExpr *LiteralExpr::CreateInt(AstContext &ctx, int64_t value, IntSuffix suffix) {
  return AllocateNode<LiteralExpr>(ctx, Kind::kInt, value, suffix, false, StringRef());
}

LiteralExpr *LiteralExpr::CreateBool(AstContext &ctx, bool value) {
  return AllocateNode<LiteralExpr>(ctx, Kind::kBool, 0, IntSuffix::kNone, value, StringRef());
}

LiteralExpr *LiteralExpr::CreateString(AstContext &ctx, const std::string &value) {
  return AllocateNode<LiteralExpr>(ctx, Kind::kString, 0, IntSuffix::kNone, false, ctx.CopyString(value.c_str()));
}

LiteralExpr *LiteralExpr::CreateNullptr(AstContext &ctx) {
  return AllocateNode<LiteralExpr>(ctx, Kind::kNullptr, 0, IntSuffix::kNone, false, StringRef());
}

Status LiteralExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

AssignExpr *AssignExpr::Create(AstContext &ctx, Expr *lhs, Expr *rhs) {
  GE_ASSERT_NOTNULL(lhs);
  GE_ASSERT_NOTNULL(rhs);
  return AllocateNode<AssignExpr>(ctx, lhs, rhs);
}

Status AssignExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

BinaryExpr *BinaryExpr::Create(AstContext &ctx, Op op, Expr *lhs, Expr *rhs) {
  GE_ASSERT_NOTNULL(lhs);
  GE_ASSERT_NOTNULL(rhs);
  return AllocateNode<BinaryExpr>(ctx, op, lhs, rhs);
}

Status BinaryExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

UnaryExpr *UnaryExpr::Create(AstContext &ctx, Op op, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<UnaryExpr>(ctx, op, expr);
}

Status UnaryExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

CallExpr *CallExpr::Create(AstContext &ctx, Expr *callee, const std::vector<Expr *> &args) {
  GE_ASSERT_NOTNULL(callee);
  return AllocateNode<CallExpr>(ctx, callee, CopyArray(ctx, args));
}

Status CallExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

MakeUniqueArrayExpr *MakeUniqueArrayExpr::Create(AstContext &ctx, const TypeName &elem_type, Expr *count) {
  GE_ASSERT_NOTNULL(count);
  return AllocateNode<MakeUniqueArrayExpr>(ctx, elem_type, count);
}

Status MakeUniqueArrayExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ToStrExpr *ToStrExpr::Create(AstContext &ctx, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<ToStrExpr>(ctx, expr);
}

Status ToStrExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

MemcpyExpr *MemcpyExpr::Create(AstContext &ctx, Expr *dst, Expr *src, Expr *size) {
  GE_ASSERT_NOTNULL(dst);
  GE_ASSERT_NOTNULL(src);
  GE_ASSERT_NOTNULL(size);
  return AllocateNode<MemcpyExpr>(ctx, dst, src, size);
}

Status MemcpyExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

SizeofExpr *SizeofExpr::Create(AstContext &ctx, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<SizeofExpr>(ctx, expr);
}

Status SizeofExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

RemoveFileExpr *RemoveFileExpr::Create(AstContext &ctx, Expr *path) {
  GE_ASSERT_NOTNULL(path);
  return AllocateNode<RemoveFileExpr>(ctx, path);
}

Status RemoveFileExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

IgnoreOutputExpr *IgnoreOutputExpr::Create(AstContext &ctx, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<IgnoreOutputExpr>(ctx, expr);
}

Status IgnoreOutputExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ContainerMethodExpr *ContainerMethodExpr::Create(AstContext &ctx, Method method, Expr *container,
                                                 const std::vector<Expr *> &args) {
  GE_ASSERT_NOTNULL(container);
  return AllocateNode<ContainerMethodExpr>(ctx, method, container, CopyArray(ctx, args));
}

Status ContainerMethodExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

AddrOfExpr *AddrOfExpr::Create(AstContext &ctx, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<AddrOfExpr>(ctx, expr);
}

Status AddrOfExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

SubscriptExpr *SubscriptExpr::Create(AstContext &ctx, Expr *base, Expr *index) {
  GE_ASSERT_NOTNULL(base);
  GE_ASSERT_NOTNULL(index);
  return AllocateNode<SubscriptExpr>(ctx, base, index);
}

Status SubscriptExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

MemberExpr *MemberExpr::Create(AstContext &ctx, Expr *object, const std::string &field) {
  GE_ASSERT_NOTNULL(object);
  return AllocateNode<MemberExpr>(ctx, object, ctx.CopyString(field.c_str()));
}

Status MemberExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

CppArrowMemberExpr *CppArrowMemberExpr::Create(AstContext &ctx, Expr *object, const std::string &field) {
  GE_ASSERT_NOTNULL(object);
  return AllocateNode<CppArrowMemberExpr>(ctx, object, ctx.CopyString(field.c_str()));
}

Status CppArrowMemberExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

CppCastExpr *CppCastExpr::Create(AstContext &ctx, Kind kind, const std::string &target_type, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<CppCastExpr>(ctx, kind, ctx.CopyString(target_type.c_str()), expr);
}

Status CppCastExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

LambdaExpr *LambdaExpr::Create(AstContext &ctx, const std::vector<std::string> &captures, BlockStmt *body) {
  GE_ASSERT_NOTNULL(body);
  std::vector<StringRef> copied;
  copied.reserve(captures.size());
  for (const auto &capture : captures) {
    copied.push_back(ctx.CopyString(capture.c_str()));
  }
  return AllocateNode<LambdaExpr>(ctx, CopyArray(ctx, copied), body);
}

Status LambdaExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

InitListExpr *InitListExpr::Create(AstContext &ctx, const std::vector<Expr *> &elements, bool compact) {
  return AllocateNode<InitListExpr>(ctx, CopyArray(ctx, elements), compact);
}

Status InitListExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

CCastExpr *CCastExpr::Create(AstContext &ctx, const std::string &target_type, Expr *expr) {
  return AllocateNode<CCastExpr>(ctx, ctx.CopyString(target_type.c_str()), expr);
}

Status CCastExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

DesignatedInitListExpr *DesignatedInitListExpr::Create(AstContext &ctx, const std::vector<std::string> &names,
                                                       const std::vector<Expr *> &values, bool compact) {
  auto *arena = static_cast<StringRef *>(ctx.Allocate(names.size() * sizeof(StringRef)));
  for (size_t i = 0U; i < names.size(); ++i) {
    new (&arena[i]) StringRef(ctx.CopyString(names[i].c_str()));
  }
  return AllocateNode<DesignatedInitListExpr>(ctx, ArrayRef<StringRef>(arena, names.size()), CopyArray(ctx, values),
                                              compact);
}

Status DesignatedInitListExpr::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

CommentStmt *CommentStmt::Create(AstContext &ctx, const std::string &text) {
  return AllocateNode<CommentStmt>(ctx, ctx.CopyString(text.c_str()));
}

Status CommentStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

BlankLineStmt *BlankLineStmt::Create(AstContext &ctx) {
  return AllocateNode<BlankLineStmt>(ctx);
}

Status BlankLineStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

VarDeclStmt *VarDeclStmt::Create(AstContext &ctx, const std::string &type_spec, const std::string &name, Expr *init) {
  return AllocateNode<VarDeclStmt>(ctx, ctx.CopyString(type_spec.c_str()), ctx.CopyString(name.c_str()), init);
}

Status VarDeclStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ExprStmt *ExprStmt::Create(AstContext &ctx, Expr *expr) {
  GE_ASSERT_NOTNULL(expr);
  return AllocateNode<ExprStmt>(ctx, expr);
}

Status ExprStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ReturnStmt *ReturnStmt::Create(AstContext &ctx, Expr *value) {
  return AllocateNode<ReturnStmt>(ctx, value);
}

Status ReturnStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

BlockStmt *BlockStmt::Create(AstContext &ctx, const std::vector<Stmt *> &statements) {
  return AllocateNode<BlockStmt>(ctx, CopyArray(ctx, statements));
}

Status BlockStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

IfStmt *IfStmt::Create(AstContext &ctx, Expr *cond, BlockStmt *then_block, BlockStmt *else_block,
                       bool is_preprocessor) {
  GE_ASSERT_NOTNULL(cond);
  GE_ASSERT_NOTNULL(then_block);
  return AllocateNode<IfStmt>(ctx, cond, then_block, else_block, is_preprocessor);
}

Status IfStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

ForStmt *ForStmt::Create(AstContext &ctx, Stmt *init, Expr *cond, Expr *step, BlockStmt *body) {
  GE_ASSERT_NOTNULL(init);
  GE_ASSERT_NOTNULL(cond);
  GE_ASSERT_NOTNULL(step);
  GE_ASSERT_NOTNULL(body);
  return AllocateNode<ForStmt>(ctx, init, cond, step, body);
}

Status ForStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

RangeForStmt *RangeForStmt::Create(AstContext &ctx, const std::string &type_spec, const std::string &name, Expr *range,
                                   BlockStmt *body) {
  GE_ASSERT_NOTNULL(range);
  GE_ASSERT_NOTNULL(body);
  return AllocateNode<RangeForStmt>(ctx, ctx.CopyString(type_spec.c_str()), ctx.CopyString(name.c_str()), range, body);
}

Status RangeForStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

CaseStmt *CaseStmt::Create(AstContext &ctx, Expr *value) {
  return AllocateNode<CaseStmt>(ctx, value);
}
Status CaseStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

BreakStmt *BreakStmt::Create(AstContext &ctx) {
  return AllocateNode<BreakStmt>(ctx);
}
Status BreakStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}

SwitchStmt *SwitchStmt::Create(AstContext &ctx, Expr *cond, BlockStmt *body) {
  return AllocateNode<SwitchStmt>(ctx, cond, body);
}
Status SwitchStmt::Accept(CodeEmitter &emitter, std::string &output) const {
  return emitter.Emit(*this, output);
}
}  // namespace ge
