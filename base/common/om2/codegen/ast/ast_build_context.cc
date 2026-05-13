/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/ast/ast_build_context.h"

namespace ge {
namespace {
Expr *ResolveArg(const Arg &arg, AstContext &ctx) { return arg.Resolve(ctx); }

Stmt *ResolveBodyItem(const BodyItem &item, AstContext &ctx) { return item.Resolve(ctx); }

ExprRef BuildCallExpr(AstContext &ctx, Expr *callee, const std::vector<Arg> &args) {
  std::vector<Expr *> resolved;
  resolved.reserve(args.size());
  for (const auto &arg : args) {
    resolved.push_back(ResolveArg(arg, ctx));
  }
  return ExprRef(ctx, CallExpr::Create(ctx, callee, resolved));
}

ExprRef BuildCallExpr(AstContext &ctx, Expr *callee, std::initializer_list<Arg> args) {
  return BuildCallExpr(ctx, callee, std::vector<Arg>(args.begin(), args.end()));
}

ExprRef BuildBinaryExpr(AstContext &ctx, BinaryExpr::Op op, Expr *lhs, Expr *rhs) {
  return ExprRef(ctx, BinaryExpr::Create(ctx, op, lhs, rhs));
}

ExprRef BuildUnaryExpr(AstContext &ctx, UnaryExpr::Op op, Expr *expr) {
  return ExprRef(ctx, UnaryExpr::Create(ctx, op, expr));
}

ExprRef BuildContainerMethodExpr(AstContext &ctx, ContainerMethodExpr::Method method, Arg container,
                                 std::initializer_list<Arg> args) {
  std::vector<Expr *> resolved;
  resolved.reserve(args.size());
  for (const auto &arg : args) {
    resolved.push_back(ResolveArg(arg, ctx));
  }
  return ExprRef(ctx, ContainerMethodExpr::Create(ctx, method, ResolveArg(container, ctx), resolved));
}

std::string BuildLambdaCapture(const LambdaCaptureSpec &capture) {
  if (capture.kind == LambdaCaptureSpec::Kind::kByRef) {
    return "&" + capture.name;
  }
  return capture.name;
}

StablePartRole InferStablePartRole(StablePartId id) {
  switch (id) {
    case StablePartId::kChkStatusMacro:
    case StablePartId::kChkNotNullMacro:
    case StablePartId::kChkTrueMacro:
    case StablePartId::kGetAddrMacro:
    case StablePartId::kMakeGuardMacro:
      return StablePartRole::kMacroGroup;
    case StablePartId::kInterfaceMacros:
      return StablePartRole::kMacroGroup;
    case StablePartId::kPointerHelpers:
    case StablePartId::kFlattenHostArgs:
    case StablePartId::kInterfacePointerHelpers:
    case StablePartId::kReadBinaryFileToBuffer:
    case StablePartId::kGenerateJsonFile:
    case StablePartId::kLoadAndRunExternalApis:
    case StablePartId::kInterfaceDumpApis:
    case StablePartId::kLoadAndRunDumpHelpers:
      return StablePartRole::kHelperFunctionGroup;
    case StablePartId::kScopeGuard:
      return StablePartRole::kHelperType;
    default:
      return StablePartRole::kHelperFunctionGroup;
  }
}

std::vector<ParamDecl *> BuildParamDecls(AstContext &ctx, const std::vector<VarRef> &params) {
  std::vector<ParamDecl *> built;
  built.reserve(params.size());
  for (const auto &param : params) {
    built.push_back(ParamDecl::Create(ctx, param.TypeName(), param.SymbolName()));
  }
  return built;
}

std::pair<std::vector<std::string>, std::vector<Expr *>> BuildMemberInits(AstContext &ctx,
                                                                          const std::vector<MemberInitSpec> &member_inits) {
  std::vector<std::string> names;
  std::vector<Expr *> exprs;
  names.reserve(member_inits.size());
  exprs.reserve(member_inits.size());
  for (const auto &member_init : member_inits) {
    names.push_back(member_init.member_name);
    exprs.push_back(ResolveArg(member_init.init, ctx));
  }
  return {names, exprs};
}
}  // namespace

Arg::Arg() : kind_(Kind::kEmpty), int_value_(0), bool_value_(false), expr_(nullptr) {}
Arg::Arg(const char *text) : kind_(Kind::kIdentifier), text_(text), int_value_(0), bool_value_(false), expr_(nullptr) {}
Arg::Arg(const std::string &text) : kind_(Kind::kIdentifier), text_(text), int_value_(0), bool_value_(false), expr_(nullptr) {}
Arg::Arg(bool value) : kind_(Kind::kBool), int_value_(0), bool_value_(value), expr_(nullptr) {}
Arg::Arg(std::nullptr_t) : kind_(Kind::kNullptr), int_value_(0), bool_value_(false), expr_(nullptr) {}
Arg::Arg(std::initializer_list<Arg> items)
    : kind_(Kind::kInitList), int_value_(0), bool_value_(false), expr_(nullptr), init_list_items_(items) {}
Arg::Arg(const std::vector<Arg> &items)
    : kind_(Kind::kInitList), int_value_(0), bool_value_(false), expr_(nullptr), init_list_items_(items) {}
Arg::Arg(Expr *expr) : kind_(Kind::kExpr), int_value_(0), bool_value_(false), expr_(expr) {}
Arg::Arg(const ExprRef &expr) : Arg(expr.Get()) {}
Arg::Arg(const VarRef &symbol) : Arg(symbol.Get()) {}

Arg Arg::StringLiteral(const std::string &text) {
  Arg arg;
  arg.kind_ = Kind::kStringLiteral;
  arg.text_ = text;
  return arg;
}

bool Arg::Empty() const { return kind_ == Kind::kEmpty; }

Expr *Arg::Resolve(AstContext &ctx) const {
  switch (kind_) {
    case Kind::kEmpty:
      return nullptr;
    case Kind::kExpr:
      return expr_;
    case Kind::kIdentifier:
      return IdentifierExpr::Create(ctx, text_);
    case Kind::kStringLiteral:
      return LiteralExpr::CreateString(ctx, text_);
    case Kind::kInt:
      return LiteralExpr::CreateInt(ctx, int_value_);
    case Kind::kBool:
      return LiteralExpr::CreateBool(ctx, bool_value_);
    case Kind::kNullptr:
      return LiteralExpr::CreateNullptr(ctx);
    case Kind::kInitList: {
      std::vector<Expr *> resolved;
      resolved.reserve(init_list_items_.size());
      for (const auto &item : init_list_items_) {
        resolved.push_back(item.Resolve(ctx));
      }
      return InitListExpr::Create(ctx, resolved);
    }
    default:
      return nullptr;
  }
}

ExprRef::ExprRef(AstContext &ctx, Expr *expr) : ctx_(&ctx), expr_(expr) {}
Expr *ExprRef::Get() const { return expr_; }
AstContext *ExprRef::Ctx() const { return ctx_; }
ExprRef ExprRef::Attr(const std::string &field) const {
  return ExprRef(*ctx_, MemberExpr::Create(*ctx_, expr_, field));
}
ExprRef ExprRef::Arrow(const std::string &field) const {
  return ExprRef(*ctx_, CppArrowMemberExpr::Create(*ctx_, expr_, field));
}
ExprRef ExprRef::Addr() const { return ExprRef(*ctx_, AddrOfExpr::Create(*ctx_, expr_)); }
ExprRef ExprRef::operator()() const { return BuildCallExpr(*ctx_, expr_, {}); }
ExprRef ExprRef::operator()(std::initializer_list<Arg> args) const { return BuildCallExpr(*ctx_, expr_, args); }
ExprRef ExprRef::operator[](Arg index) const {
  return ExprRef(*ctx_, SubscriptExpr::Create(*ctx_, expr_, ResolveArg(index, *ctx_)));
}
ExprRef ExprRef::Clear() const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kClear, *this, {});
}
ExprRef ExprRef::Resize(Arg size) const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kResize, *this, {size});
}
ExprRef ExprRef::Size() const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kSize, *this, {});
}
ExprRef ExprRef::Data() const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kData, *this, {});
}
ExprRef ExprRef::Empty() const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kEmpty, *this, {});
}
ExprRef ExprRef::At(Arg index) const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kAt, *this, {index});
}
ExprRef ExprRef::PushBack(Arg value) const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kPushBack, *this, {value});
}
ExprRef ExprRef::CStr() const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kCStr, *this, {});
}
ExprRef ExprRef::GetPtr() const {
  return BuildContainerMethodExpr(*ctx_, ContainerMethodExpr::Method::kGetPtr, *this, {});
}

ExprRef operator!(ExprRef expr) { return BuildUnaryExpr(*expr.Ctx(), UnaryExpr::Op::kLogicalNot, expr.Get()); }
ExprRef operator-(ExprRef expr) { return BuildUnaryExpr(*expr.Ctx(), UnaryExpr::Op::kNegate, expr.Get()); }
ExprRef operator~(ExprRef expr) { return BuildUnaryExpr(*expr.Ctx(), UnaryExpr::Op::kBitNot, expr.Get()); }

#define DEFINE_FREE_BINARY_OP(op_symbol, op_value) \
  ExprRef operator op_symbol(ExprRef lhs, Arg rhs) { \
    return BuildBinaryExpr(*lhs.Ctx(), op_value, lhs.Get(), ResolveArg(rhs, *lhs.Ctx())); \
  }
DEFINE_FREE_BINARY_OP(==, BinaryExpr::Op::kEq)
DEFINE_FREE_BINARY_OP(!=, BinaryExpr::Op::kNe)
DEFINE_FREE_BINARY_OP(<, BinaryExpr::Op::kLt)
DEFINE_FREE_BINARY_OP(<=, BinaryExpr::Op::kLe)
DEFINE_FREE_BINARY_OP(>, BinaryExpr::Op::kGt)
DEFINE_FREE_BINARY_OP(>=, BinaryExpr::Op::kGe)
DEFINE_FREE_BINARY_OP(&&, BinaryExpr::Op::kLogicalAnd)
DEFINE_FREE_BINARY_OP(||, BinaryExpr::Op::kLogicalOr)
DEFINE_FREE_BINARY_OP(+, BinaryExpr::Op::kAdd)
DEFINE_FREE_BINARY_OP(-, BinaryExpr::Op::kSub)
DEFINE_FREE_BINARY_OP(*, BinaryExpr::Op::kMul)
DEFINE_FREE_BINARY_OP(/, BinaryExpr::Op::kDiv)
DEFINE_FREE_BINARY_OP(%, BinaryExpr::Op::kMod)
DEFINE_FREE_BINARY_OP(&, BinaryExpr::Op::kBitAnd)
DEFINE_FREE_BINARY_OP(|, BinaryExpr::Op::kBitOr)
DEFINE_FREE_BINARY_OP(^, BinaryExpr::Op::kBitXor)
DEFINE_FREE_BINARY_OP(<<, BinaryExpr::Op::kShiftLeft)
DEFINE_FREE_BINARY_OP(>>, BinaryExpr::Op::kShiftRight)
#undef DEFINE_FREE_BINARY_OP

VarRef::VarRef(AstContext &ctx, std::string type_name, const std::string &symbol_name)
    : ExprRef(ctx, IdentifierExpr::Create(ctx, symbol_name)),
      type_name_(std::move(type_name)),
      symbol_name_(symbol_name) {}

const std::string &VarRef::TypeName() const { return type_name_; }
const std::string &VarRef::SymbolName() const { return symbol_name_; }

BodyItem::BodyItem(Stmt *stmt) : stmt_(stmt), expr_(nullptr) {}
BodyItem::BodyItem(Expr *expr) : stmt_(nullptr), expr_(expr) {}
BodyItem::BodyItem(const ExprRef &expr) : BodyItem(expr.Get()) {}
BodyItem::BodyItem(const VarRef &symbol) : BodyItem(symbol.Get()) {}

Stmt *BodyItem::Resolve(AstContext &ctx) const {
  if (stmt_ != nullptr) {
    return stmt_;
  }
  GE_ASSERT_NOTNULL(expr_);
  return ExprStmt::Create(ctx, expr_);
}

AstBuildContext::AstBuildContext(AstContext &ctx) : ctx_(ctx) {}

TranslationUnit *AstBuildContext::File(const std::vector<DeclNode *> &items) { return ::ge::TranslationUnit::Create(ctx_, items); }

TranslationUnit *AstBuildContext::File(std::initializer_list<DeclNode *> items) {
  return File(std::vector<DeclNode *>(items.begin(), items.end()));
}

IncludeDecl *AstBuildContext::Include(const std::string &path, IncludeDecl::Kind kind) {
  return IncludeDecl::Create(ctx_, path, kind);
}

SpaceDecl *AstBuildContext::Space() { return SpaceDecl::Create(ctx_); }
CommentStmt *AstBuildContext::Comment(const std::string &text) { return CommentStmt::Create(ctx_, text); }
BlankLineStmt *AstBuildContext::BlankLine() { return BlankLineStmt::Create(ctx_); }

AccessSectionDecl *AstBuildContext::Public() { return AccessSectionDecl::Create(ctx_, AccessSectionDecl::Kind::kPublic); }
AccessSectionDecl *AstBuildContext::Private() { return AccessSectionDecl::Create(ctx_, AccessSectionDecl::Kind::kPrivate); }
Expr *AstBuildContext::Str(const std::string &text) { return LiteralExpr::CreateString(ctx_, text); }
Expr *AstBuildContext::UInt(uint64_t value) {
  return LiteralExpr::CreateInt(ctx_, static_cast<int64_t>(value), LiteralExpr::IntSuffix::kU);
}
Expr *AstBuildContext::ULong(uint64_t value) {
  return LiteralExpr::CreateInt(ctx_, static_cast<int64_t>(value), LiteralExpr::IntSuffix::kUL);
}

TypeAliasDecl *AstBuildContext::TypeAlias(const std::string &type_spec, const std::string &name) {
  return TypeAliasDecl::Create(ctx_, type_spec, name);
}

FieldDecl *AstBuildContext::Field(const std::string &type_spec, const std::string &name, Arg init) {
  return FieldDecl::Create(ctx_, type_spec, name, ResolveArg(init, ctx_));
}

ClassDecl *AstBuildContext::Class(const std::string &name, const std::vector<DeclNode *> &items) {
  return ClassDecl::Create(ctx_, name, items);
}

ClassDecl *AstBuildContext::Class(const std::string &name, std::initializer_list<DeclNode *> items) {
  return Class(name, std::vector<DeclNode *>(items.begin(), items.end()));
}

StructDecl *AstBuildContext::Struct(const std::string &name, const std::vector<DeclNode *> &items) {
  return StructDecl::Create(ctx_, name, items);
}

StructDecl *AstBuildContext::Struct(const std::string &name, std::initializer_list<DeclNode *> items) {
  return Struct(name, std::vector<DeclNode *>(items.begin(), items.end()));
}

NamespaceDecl *AstBuildContext::Namespace(const std::string &name, const std::vector<DeclNode *> &items) {
  return NamespaceDecl::Create(ctx_, name, items);
}

NamespaceDecl *AstBuildContext::Namespace(const std::string &name, std::initializer_list<DeclNode *> items) {
  return Namespace(name, std::vector<DeclNode *>(items.begin(), items.end()));
}

ExternBlockDecl *AstBuildContext::ExternBlock(const std::string &lang, const std::vector<DeclNode *> &items) {
  return ExternBlockDecl::Create(ctx_, lang, items);
}

ExternBlockDecl *AstBuildContext::ExternBlock(const std::string &lang, std::initializer_list<DeclNode *> items) {
  return ExternBlock(lang, std::vector<DeclNode *>(items.begin(), items.end()));
}

StablePartDecl *AstBuildContext::StablePart(StablePartId id, StablePartPlacement placement) {
  return StablePartDecl::Create(ctx_, id, InferStablePartRole(id), placement);
}

FunctionDecl *AstBuildContext::DeclareFunction(const std::string &name, const std::vector<VarRef> &params,
                                               const std::string &return_type) {
  return ::ge::FunctionDecl::Create(ctx_, name, BuildParamDecls(ctx_, params), return_type);
}

FunctionDecl *AstBuildContext::DeclareFunction(const std::string &name, std::initializer_list<VarRef> params,
                                               const std::string &return_type) {
  return DeclareFunction(name, std::vector<VarRef>(params.begin(), params.end()), return_type);
}

MethodDecl *AstBuildContext::DeclareMethod(const std::string &name, const std::vector<VarRef> &params,
                                           const std::string &return_type, const std::string &trailing_spec) {
  return ::ge::MethodDecl::Create(ctx_, name, BuildParamDecls(ctx_, params), return_type, trailing_spec);
}

MethodDecl *AstBuildContext::DeclareMethod(const std::string &name, std::initializer_list<VarRef> params,
                                           const std::string &return_type, const std::string &trailing_spec) {
  return DeclareMethod(name, std::vector<VarRef>(params.begin(), params.end()), return_type, trailing_spec);
}

FunctionDef *AstBuildContext::DefineFunction(const std::string &name, const std::vector<VarRef> &params,
                                             const std::string &return_type, const std::vector<Stmt *> &body) {
  return ::ge::FunctionDef::Create(ctx_, name, BuildParamDecls(ctx_, params), return_type, BlockStmt::Create(ctx_, body));
}

FunctionDef *AstBuildContext::DefineFunction(const std::string &name, std::initializer_list<VarRef> params,
                                             const std::string &return_type, std::initializer_list<BodyItem> items) {
  return DefineFunction(name, std::vector<VarRef>(params.begin(), params.end()), return_type, Body(items));
}

MethodDef *AstBuildContext::DefineMethod(const std::string &owner, const std::string &name,
                                         const std::vector<VarRef> &params, const std::string &return_type,
                                         const std::vector<Stmt *> &body) {
  return ::ge::MethodDef::Create(ctx_, owner, name, BuildParamDecls(ctx_, params), return_type, std::vector<std::string>{},
                                 std::vector<Expr *>{},
                                 BlockStmt::Create(ctx_, body));
}

MethodDef *AstBuildContext::DefineMethod(const std::string &owner, const std::string &name,
                                         const std::vector<VarRef> &params, const std::string &return_type,
                                         const std::vector<BodyItem> &items) {
  return DefineMethod(owner, name, params, return_type, Body(items));
}

MethodDef *AstBuildContext::DefineMethod(const std::string &owner, const std::string &name,
                                         std::initializer_list<VarRef> params, const std::string &return_type,
                                         std::initializer_list<BodyItem> items) {
  return DefineMethod(owner, name, std::vector<VarRef>(params.begin(), params.end()), return_type, Body(items));
}

MethodDef *AstBuildContext::DefineMethod(const std::string &owner, const std::string &name,
                                         const std::vector<VarRef> &params, const std::string &return_type,
                                         const std::vector<MemberInitSpec> &member_inits,
                                         const std::vector<Stmt *> &body) {
  const auto built_member_inits = BuildMemberInits(ctx_, member_inits);
  return ::ge::MethodDef::Create(ctx_, owner, name, BuildParamDecls(ctx_, params), return_type,
                                 built_member_inits.first, built_member_inits.second, BlockStmt::Create(ctx_, body));
}

MethodDef *AstBuildContext::DefineMethod(const std::string &owner, const std::string &name,
                                         const std::vector<VarRef> &params, const std::string &return_type,
                                         const std::vector<MemberInitSpec> &member_inits,
                                         const std::vector<BodyItem> &items) {
  return DefineMethod(owner, name, params, return_type, member_inits, Body(items));
}

MethodDef *AstBuildContext::DefineMethod(const std::string &owner, const std::string &name,
                                         std::initializer_list<VarRef> params, const std::string &return_type,
                                         std::initializer_list<MemberInitSpec> member_inits,
                                         std::initializer_list<BodyItem> items) {
  return DefineMethod(owner, name, std::vector<VarRef>(params.begin(), params.end()), return_type,
                      std::vector<MemberInitSpec>(member_inits.begin(), member_inits.end()), Body(items));
}

InitListExpr *AstBuildContext::InitList(std::initializer_list<Arg> items) {
  return InitList(std::vector<Arg>(items.begin(), items.end()));
}

InitListExpr *AstBuildContext::InitList(const std::vector<Arg> &items) {
  std::vector<Expr *> resolved;
  resolved.reserve(items.size());
  for (const auto &item : items) {
    resolved.push_back(ResolveArg(item, ctx_));
  }
  return InitListExpr::Create(ctx_, resolved);
}

VarRef AstBuildContext::Var(const std::string &type_name, const std::string &symbol_name) {
  return VarRef(ctx_, type_name, symbol_name);
}

ExprRef AstBuildContext::Assign(Arg lhs, Arg rhs) {
  return ExprRef(ctx_, AssignExpr::Create(ctx_, ResolveArg(lhs, ctx_), ResolveArg(rhs, ctx_)));
}

ExprRef AstBuildContext::Deref(Arg expr) {
  return ExprRef(ctx_, UnaryExpr::Create(ctx_, UnaryExpr::Op::kDeref, ResolveArg(expr, ctx_)));
}

ExprRef AstBuildContext::PreInc(Arg expr) {
  return ExprRef(ctx_, UnaryExpr::Create(ctx_, UnaryExpr::Op::kPreInc, ResolveArg(expr, ctx_)));
}

ExprRef AstBuildContext::PostInc(Arg expr) {
  return ExprRef(ctx_, UnaryExpr::Create(ctx_, UnaryExpr::Op::kPostInc, ResolveArg(expr, ctx_)));
}

ExprRef AstBuildContext::ToStr(Arg expr) { return ExprRef(ctx_, ToStrExpr::Create(ctx_, ResolveArg(expr, ctx_))); }

ExprRef AstBuildContext::Memcpy(Arg dst, Arg src, Arg size) {
  return ExprRef(ctx_, MemcpyExpr::Create(ctx_, ResolveArg(dst, ctx_), ResolveArg(src, ctx_), ResolveArg(size, ctx_)));
}

ExprRef AstBuildContext::Sizeof(Arg expr) { return ExprRef(ctx_, SizeofExpr::Create(ctx_, ResolveArg(expr, ctx_))); }

ExprRef AstBuildContext::RemoveFile(Arg path) {
  return ExprRef(ctx_, RemoveFileExpr::Create(ctx_, ResolveArg(path, ctx_)));
}

ExprRef AstBuildContext::IgnoreOutput(Arg expr) {
  return ExprRef(ctx_, IgnoreOutputExpr::Create(ctx_, ResolveArg(expr, ctx_)));
}

LambdaCaptureSpec AstBuildContext::CaptureRef(const VarRef &symbol) const {
  return LambdaCaptureSpec{symbol.SymbolName(), LambdaCaptureSpec::Kind::kByRef};
}

ExprRef AstBuildContext::Lambda(std::initializer_list<LambdaCaptureSpec> captures,
                                std::initializer_list<BodyItem> items) {
  std::vector<std::string> built_captures;
  built_captures.reserve(captures.size());
  for (const auto &capture : captures) {
    built_captures.push_back(BuildLambdaCapture(capture));
  }
  return ExprRef(ctx_, LambdaExpr::Create(ctx_, built_captures, BlockStmt::Create(ctx_, Body(items))));
}

ExprRef AstBuildContext::Call(const std::string &callee_name, std::initializer_list<Arg> args) {
  return BuildCallExpr(ctx_, IdentifierExpr::Create(ctx_, callee_name), args);
}

ExprRef AstBuildContext::Call(const std::string &callee_name, const std::vector<Arg> &args) {
  return BuildCallExpr(ctx_, IdentifierExpr::Create(ctx_, callee_name), args);
}

ExprRef AstBuildContext::MakeUniqueArray(BuiltinType elem_type, Arg count) {
  return MakeUniqueArray(TypeName(elem_type), count);
}

ExprRef AstBuildContext::MakeUniqueArray(const std::string &elem_type, Arg count) {
  return MakeUniqueArray(TypeName(ctx_.CopyString(elem_type.c_str())), count);
}

ExprRef AstBuildContext::MakeUniqueArray(const char *elem_type, Arg count) {
  return MakeUniqueArray(std::string(elem_type == nullptr ? "" : elem_type), count);
}

ExprRef AstBuildContext::MakeUniqueArray(const TypeName &elem_type, Arg count) {
  return ExprRef(ctx_, MakeUniqueArrayExpr::Create(ctx_, elem_type, ResolveArg(count, ctx_)));
}

ReturnStmt *AstBuildContext::Return(Arg value) { return ReturnStmt::Create(ctx_, ResolveArg(value, ctx_)); }

VarDeclStmt *AstBuildContext::VarDecl(const std::string &type_spec, const std::string &name, Arg init) {
  return VarDeclStmt::Create(ctx_, type_spec, name, ResolveArg(init, ctx_));
}

VarDeclStmt *AstBuildContext::VarDecl(const VarRef &symbol, Arg init) {
  return VarDecl(symbol.TypeName(), symbol.SymbolName(), init);
}

IfStmt *AstBuildContext::If(Arg cond, std::initializer_list<BodyItem> then_items) {
  return IfStmt::Create(ctx_, ResolveArg(cond, ctx_), BlockStmt::Create(ctx_, Body(then_items)));
}

IfStmt *AstBuildContext::If(Arg cond, std::initializer_list<BodyItem> then_items,
                            std::initializer_list<BodyItem> else_items) {
  return IfStmt::Create(ctx_, ResolveArg(cond, ctx_), BlockStmt::Create(ctx_, Body(then_items)),
                        BlockStmt::Create(ctx_, Body(else_items)));
}

ForStmt *AstBuildContext::For(Stmt *init, Arg cond, Arg step, std::initializer_list<BodyItem> items) {
  return ForStmt::Create(ctx_, init, ResolveArg(cond, ctx_), ResolveArg(step, ctx_), BlockStmt::Create(ctx_, Body(items)));
}

RangeForStmt *AstBuildContext::RangeFor(const VarRef &loop_var, Arg range, std::initializer_list<BodyItem> items) {
  return RangeFor(loop_var.TypeName(), loop_var.SymbolName(), range, items);
}

RangeForStmt *AstBuildContext::RangeFor(const std::string &type_spec, const std::string &name, Arg range,
                                        std::initializer_list<BodyItem> items) {
  return RangeForStmt::Create(ctx_, type_spec, name, ResolveArg(range, ctx_), BlockStmt::Create(ctx_, Body(items)));
}

MemberInitSpec AstBuildContext::MemberInit(const std::string &member_name, Arg init) const {
  return MemberInitSpec{member_name, init};
}

std::vector<Stmt *> AstBuildContext::Body(std::initializer_list<BodyItem> items) {
  return Body(std::vector<BodyItem>(items.begin(), items.end()));
}

std::vector<Stmt *> AstBuildContext::Body(const std::vector<BodyItem> &items) {
  std::vector<Stmt *> body;
  body.reserve(items.size());
  for (const auto &item : items) {
    body.push_back(ResolveBodyItem(item, ctx_));
  }
  return body;
}

ExprRef AstBuildContext::StaticCast(const std::string &target_type, Arg expr) {
  return ExprRef(ctx_, CppCastExpr::Create(ctx_, CppCastExpr::Kind::kStatic, target_type, ResolveArg(expr, ctx_)));
}

ExprRef AstBuildContext::ConstCast(const std::string &target_type, Arg expr) {
  return ExprRef(ctx_, CppCastExpr::Create(ctx_, CppCastExpr::Kind::kConst, target_type, ResolveArg(expr, ctx_)));
}

ExprRef AstBuildContext::ReinterpretCast(const std::string &target_type, Arg expr) {
  return ExprRef(ctx_,
                 CppCastExpr::Create(ctx_, CppCastExpr::Kind::kReinterpret, target_type, ResolveArg(expr, ctx_)));
}
}  // namespace ge
