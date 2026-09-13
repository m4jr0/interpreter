#include "lox.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "logger.hpp"
#include "scanner.hpp"
#include "token.hpp"

namespace jlox {

void Lox::Run(std::string_view source) {
  Scanner scanner{source};
  auto tokens = scanner.ScanTokens();

  for (const Token &token : tokens) {
    JLOX_LOG(token);
  }
}

int Lox::RunFile(const std::filesystem::path &path) {
  std::ifstream file(path);

  if (!file) {
    JLOX_ERROR("Could not open file.");
    return 1;
  }

  std::string source{
      std::istreambuf_iterator<char>{file},
      std::istreambuf_iterator<char>{},
  };

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

    // A syntax error in one REPL line shouldn't kill the entire prompt.
    hadError_ = false;
  }
}

void Lox::Error(const std::size_t line, const std::string_view message) {
  Report(line, "", message);
}

void Lox::Report(const std::size_t line, const std::string_view where,
                 const std::string_view message) {
  std::cerr << "[line " << line << "] Error" << where << ": " << message
            << '\n';

  hadError_ = true;
}

} // namespace jlox