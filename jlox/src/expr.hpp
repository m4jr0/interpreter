#pragma once

#include <memory>
#include <utility>

#include "token.hpp"

namespace jlox {

class BinaryExpr;
class ConditionalExpr;
class GroupingExpr;
class LiteralExpr;
class UnaryExpr;

class ExprVisitor {
public:
  virtual ~ExprVisitor() = default;

  virtual void VisitBinaryExpr(const BinaryExpr &expr) = 0;
  virtual void VisitConditionalExpr(const ConditionalExpr &expr) = 0;
  virtual void VisitGroupingExpr(const GroupingExpr &expr) = 0;
  virtual void VisitLiteralExpr(const LiteralExpr &expr) = 0;
  virtual void VisitUnaryExpr(const UnaryExpr &expr) = 0;
};

class Expr {
public:
  virtual ~Expr() = default;

  virtual void Accept(ExprVisitor &visitor) const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

class BinaryExpr final : public Expr {
public:
  BinaryExpr(ExprPtr left, Token op, ExprPtr right)
      : left(std::move(left)),
        op(std::move(op)),
        right(std::move(right)) {}

  void Accept(ExprVisitor &visitor) const override {
    visitor.VisitBinaryExpr(*this);
  }

  ExprPtr left;
  Token op;
  ExprPtr right;
};

class ConditionalExpr final : public Expr {
public:
  ConditionalExpr(ExprPtr condition, ExprPtr thenBranch, ExprPtr elseBranch)
      : condition(std::move(condition)),
        thenBranch(std::move(thenBranch)),
        elseBranch(std::move(elseBranch)) {}

  void Accept(ExprVisitor &visitor) const override {
    visitor.VisitConditionalExpr(*this);
  }

  ExprPtr condition;
  ExprPtr thenBranch;
  ExprPtr elseBranch;
};

class GroupingExpr final : public Expr {
public:
  GroupingExpr(ExprPtr expression)
      : expression(std::move(expression)) {}

  void Accept(ExprVisitor &visitor) const override {
    visitor.VisitGroupingExpr(*this);
  }

  ExprPtr expression;
};

class LiteralExpr final : public Expr {
public:
  LiteralExpr(Literal value)
      : value(std::move(value)) {}

  void Accept(ExprVisitor &visitor) const override {
    visitor.VisitLiteralExpr(*this);
  }

  Literal value;
};

class UnaryExpr final : public Expr {
public:
  UnaryExpr(Token op, ExprPtr right)
      : op(std::move(op)),
        right(std::move(right)) {}

  void Accept(ExprVisitor &visitor) const override {
    visitor.VisitUnaryExpr(*this);
  }

  Token op;
  ExprPtr right;
};

} // namespace jlox
