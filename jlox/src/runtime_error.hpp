#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "token.hpp"

namespace jlox {

class RuntimeError final : public std::runtime_error {
public:
  RuntimeError(const Token &token, std::string_view message)
      : std::runtime_error(std::string(message)), token(token) {}

  Token token;
};

} // namespace jlox