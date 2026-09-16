#include "lox.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "ast_printer.hpp"
#include "interpreter.hpp"
#include "parser.hpp"
#include "scanner.hpp"

namespace jlox {

int Lox::RunFile(std::string_view path) {
  std::ifstream file(std::string(path), std::ios::binary);

  if (!file) {
    std::cerr << "Could not open file: " << path << '\n';
    return 74;
  }

  const std::string source{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};

  Run(source);

  if (hadError_) {
    std::exit(65);
  }

  if (hadRuntimeError_) {
    std::exit(70);
  }

  return 0;
}

void Lox::RunPrompt() {
  std::string line;

  while (std::cout << "> " && std::getline(std::cin, line)) {
    RunPromptLine(line);

    hadError_ = false;
    hadRuntimeError_ = false;
  }
}

void Lox::Run(std::string_view source) {
  Scanner scanner(source);
  const auto tokens = scanner.ScanTokens();

  Parser parser(tokens);
  auto statements = parser.Parse();

  if (hadError_) {
    return;
  }

  interpreter_.Interpret(statements);
}

void Lox::RunPromptLine(std::string_view source) {
  Scanner scanner(source);
  const auto tokens = scanner.ScanTokens();

  // First try the input as an expression.
  {
    Parser parser(tokens);
    ExprPtr expression = parser.ParseExpression();

    if (!hadError_ && expression) {
      interpreter_.InterpretExpression(*expression);
      return;
    }
  }

  // The failed speculative expression parse may have reported errors.
  // Clear those before trying the input as a normal program.
  hadError_ = false;

  Parser parser(tokens);
  auto statements = parser.Parse();

  if (hadError_) {
    return;
  }

  interpreter_.Interpret(statements);
}

void Lox::Error(std::size_t line, std::string_view message) {
  Report(line, "", message);
}

void Lox::Error(const Token &token, std::string_view message) {
  if (token.type == TokenType::Eof) {
    Report(token.line, " at end", message);
  } else {
    const std::string where = " at '" + std::string(token.lexeme) + "'";

    Report(token.line, where, message);
  }
}

void Lox::RuntimeError(const jlox::RuntimeError &error) {
  std::cerr << error.what() << "\n[line " << error.token.line << "]\n";
  hadRuntimeError_ = true;
}

void Lox::Report(std::size_t line, std::string_view where,
                 std::string_view message) {
  std::cerr << "[line " << line << "] Error" << where << ": " << message
            << '\n';

  hadError_ = true;
}

} // namespace jlox