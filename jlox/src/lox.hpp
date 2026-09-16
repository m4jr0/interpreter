#pragma once

#include <cstddef>
#include <string_view>

#include "interpreter.hpp"
#include "runtime_error.hpp"
#include "token.hpp"

namespace jlox {

class Lox final {
public:
  static int RunFile(std::string_view path);
  static void RunPrompt();

  static void Error(std::size_t line, std::string_view message);
  static void Error(const Token &token, std::string_view message);
  static void RuntimeError(const jlox::RuntimeError &error);

private:
  static void Run(std::string_view source);
  static void RunPromptLine(std::string_view source);

  static void Report(std::size_t line, std::string_view where,
                     std::string_view message);

  inline static Interpreter interpreter_{};

  inline static bool hadError_ = false;
  inline static bool hadRuntimeError_ = false;
};

} // namespace jlox