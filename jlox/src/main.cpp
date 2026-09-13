#include "essentials.hpp"
#include "lox.hpp"

//

#include <iostream>
#include <memory>

#include "ast_printer.hpp"
#include "expr.hpp"

//

int main(int argc, char *argv[]) {
  //

  jlox::ExprPtr expression = std::make_unique<jlox::BinaryExpr>(
      std::make_unique<jlox::UnaryExpr>(
          jlox::Token{
              jlox::TokenType::Minus,
              "-",
              std::monostate{},
              1,
          },
          std::make_unique<jlox::LiteralExpr>(123.0)),
      jlox::Token{
          jlox::TokenType::Star,
          "*",
          std::monostate{},
          1,
      },
      std::make_unique<jlox::GroupingExpr>(
          std::make_unique<jlox::LiteralExpr>(45.67)));

  jlox::AstPrinter printer;
  std::cout << printer.Print(*expression) << '\n';

  //

  if (argc > 2) {
    JLOX_ERROR("Usage: jlox [script]");
    return 64;
  }

  if (argc == 2) {
    return jlox::Lox::RunFile(argv[1]);
  }

  jlox::Lox::RunPrompt();
  return 0;
}