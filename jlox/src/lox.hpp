#pragma once

#include <cstddef>
#include <string_view>

#include "token.hpp"

namespace jlox {

class Lox final {
public:
  static int RunFile(std::string_view path);
  static void RunPrompt();

  static void Error(std::size_t line, std::string_view message);
  static void Error(const Token &token, std::string_view message);

private:
  static void Run(std::string_view source);

  static void Report(std::size_t line, std::string_view where,
                     std::string_view message);

  inline static bool hadError_ = false;
};

} // namespace jlox