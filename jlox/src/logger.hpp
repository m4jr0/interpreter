#pragma once

#include <iostream>
#include <source_location>
#include <utility>

namespace jlox {

enum class LogLevel {
  Info,
  Warning,
  Error,
};

class Logger final {
public:
  template <typename T>
  static void
  Log(T &&value, LogLevel level,
      const std::source_location location = std::source_location::current()) {
    std::ostream &stream = level == LogLevel::Error ? std::cerr : std::cout;

    stream << Prefix(level) << std::forward<T>(value);

    if (level == LogLevel::Error) {
      stream << " [" << location.file_name() << ':' << location.line() << ']';
    }

    stream << '\n';
  }

private:
  static constexpr const char *Prefix(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Info:
      return "[Info] ";

    case LogLevel::Warning:
      return "[Warning] ";

    case LogLevel::Error:
      return "[Error] ";
    }

    return "";
  }
};

} // namespace jlox

#define JLOX_LOG(value) ::jlox::Logger::Log((value), ::jlox::LogLevel::Info)

#define JLOX_WARN(value) ::jlox::Logger::Log((value), ::jlox::LogLevel::Warning)

#define JLOX_ERROR(value) ::jlox::Logger::Log((value), ::jlox::LogLevel::Error)