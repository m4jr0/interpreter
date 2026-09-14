#include "lox.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "ast_printer.hpp"
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
    return 65;
  }

  return 0;
}

void Lox::RunPrompt() {
  std::string line;

  while (std::cout << "> " && std::getline(std::cin, line)) {
    Run(line);

    hadError_ = false;
  }
}

void Lox::Run(std::string_view source) {
  Scanner scanner(source);
  const auto tokens = scanner.ScanTokens();

  Parser parser(tokens);
  ExprPtr expression = parser.Parse();

  // Stop if either scanning or parsing reported an error.
  if (hadError_) {
    return;
  }

  AstPrinter printer;
  std::cout << printer.Print(*expression) << '\n';
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

void Lox::Report(std::size_t line, std::string_view where,
                 std::string_view message) {
  std::cerr << "[line " << line << "] Error" << where << ": " << message
            << '\n';

  hadError_ = true;
}

} // namespace jlox