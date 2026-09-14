#pragma once

#include <string>
#include <string_view>

#include "expr.hpp"

namespace jlox {

class AstPrinter final : public ExprVisitor {
public:
  std::string Print(const Expr &expr);

  void VisitBinaryExpr(const BinaryExpr &expr) override;
  void VisitConditionalExpr(const ConditionalExpr &expr) override;
  void VisitGroupingExpr(const GroupingExpr &expr) override;
  void VisitLiteralExpr(const LiteralExpr &expr) override;
  void VisitUnaryExpr(const UnaryExpr &expr) override;

private:
  std::string PrintSubexpression(const Expr &expr);

  template <typename... Exprs>
  void Parenthesize(std::string_view name, const Exprs &...expressions) {
    result_ = "(";
    result_ += name;

    ((result_ += " " + PrintSubexpression(expressions)), ...);

    result_ += ')';
  }

  std::string result_;
};

} // namespace jlox