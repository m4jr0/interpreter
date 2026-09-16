#pragma once

#include "runtime_error.hpp"
#include "token.hpp"
#include "value.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace jlox {

class Environment final {
public:
  Environment() = default;

  explicit Environment(std::shared_ptr<Environment> enclosing)
      : enclosing_(std::move(enclosing)) {}

  void Define(std::string_view name, Value value) {
    values_.insert_or_assign(std::string(name), std::move(value));
  }

  [[nodiscard]] Value Get(const Token &name) const {
    const auto it = values_.find(std::string(name.lexeme));

    if (it != values_.end()) {
      return it->second;
    }

    if (enclosing_) {
      return enclosing_->Get(name);
    }

    throw RuntimeError(
        name,
        "Undefined variable '" + std::string(name.lexeme) + "'.");
  }

  void Assign(const Token &name, Value value) {
    const auto it = values_.find(std::string(name.lexeme));

    if (it != values_.end()) {
      it->second = std::move(value);
      return;
    }

    if (enclosing_) {
      enclosing_->Assign(name, std::move(value));
      return;
    }

    throw RuntimeError(
        name,
        "Undefined variable '" + std::string(name.lexeme) + "'.");
  }

private:
  std::unordered_map<std::string, Value> values_;
  std::shared_ptr<Environment> enclosing_;
};

} // namespace jlox