#include "ast_printer.hpp"

#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

namespace jlox {
std::string AstPrinter::Print(const Expr &expr) {
  expr.Accept(*this);
  return result_;
}

void AstPrinter::VisitBinaryExpr(const BinaryExpr &expr) {
  Parenthesize(expr.op.lexeme, *expr.left, *expr.right);
}

void AstPrinter::VisitConditionalExpr(const ConditionalExpr &expr) {
  Parenthesize("?:", *expr.condition, *expr.thenBranch, *expr.elseBranch);
}

void AstPrinter::VisitGroupingExpr(const GroupingExpr &expr) {
  Parenthesize("group", *expr.expression);
}

void AstPrinter::VisitLiteralExpr(const LiteralExpr &expr) {
  result_ = std::visit(
      [](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          return "nil";
        } else if constexpr (std::is_same_v<T, double>) {
          std::ostringstream stream;
          stream << value;
          return stream.str();
        } else if constexpr (std::is_same_v<T, std::string_view>) {
          return std::string(value);
        } else if constexpr (std::is_same_v<T, bool>) {
          return value ? "true" : "false";
        }
      },
      expr.value);
}

void AstPrinter::VisitUnaryExpr(const UnaryExpr &expr) {
  Parenthesize(expr.op.lexeme, *expr.right);
}

std::string AstPrinter::PrintSubexpression(const Expr &expr) {
  AstPrinter printer;
  return printer.Print(expr);
}
} // namespace jlox
