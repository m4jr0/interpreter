#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Field {
  std::string type;
  std::string name;
};

struct AstType {
  std::string name;
  std::vector<Field> fields;
};

void WriteHeader(const std::filesystem::path &outputPath,
                 const std::vector<AstType> &types) {
  std::ofstream output(outputPath);

  if (!output) {
    std::cerr << "Could not open " << outputPath << '\n';
    std::exit(1);
  }

  output << R"(#pragma once

#include <memory>
#include <utility>

#include "token.hpp"

namespace jlox {

)";

  // Forward declarations.
  for (const AstType &type : types) {
    output << "class " << type.name << "Expr;\n";
  }

  output << '\n';

  // Visitor interface.
  output << "class ExprVisitor {\n";
  output << "public:\n";
  output << "  virtual ~ExprVisitor() = default;\n\n";

  for (const AstType &type : types) {
    output << "  virtual void Visit" << type.name << "Expr(const " << type.name
           << "Expr &expr) = 0;\n";
  }

  output << "};\n\n";

  // Base expression.
  output << R"(class Expr {
public:
  virtual ~Expr() = default;

  virtual void Accept(ExprVisitor &visitor) const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

)";

  // Concrete expression types.
  for (const AstType &type : types) {
    output << "class " << type.name << "Expr final : public Expr {\n";
    output << "public:\n";

    output << "  " << type.name << "Expr(";

    for (std::size_t i = 0; i < type.fields.size(); ++i) {
      if (i != 0) {
        output << ", ";
      }

      output << type.fields[i].type << ' ' << type.fields[i].name;
    }

    output << ")\n";
    output << "      : ";

    for (std::size_t i = 0; i < type.fields.size(); ++i) {
      if (i != 0) {
        output << ",\n        ";
      }

      output << type.fields[i].name << "(std::move(" << type.fields[i].name
             << "))";
    }

    output << " {}\n\n";

    output << "  void Accept(ExprVisitor &visitor) const override {\n";
    output << "    visitor.Visit" << type.name << "Expr(*this);\n";
    output << "  }\n\n";

    for (const Field &field : type.fields) {
      output << "  " << field.type << ' ' << field.name << ";\n";
    }

    output << "};\n\n";
  }

  output << "} // namespace jlox\n";
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: generate_ast <output directory>\n";
    return 64;
  }

  const std::filesystem::path outputDirectory = argv[1];

  std::filesystem::create_directories(outputDirectory);

  const std::vector<AstType> expressionTypes{
      {
          "Binary",
          {
              {"ExprPtr", "left"},
              {"Token", "op"},
              {"ExprPtr", "right"},
          },
      },
      {
          "Grouping",
          {
              {"ExprPtr", "expression"},
          },
      },
      {
          "Literal",
          {
              {"Literal", "value"},
          },
      },
      {
          "Unary",
          {
              {"Token", "op"},
              {"ExprPtr", "right"},
          },
      },
  };

  const std::filesystem::path outputPath = outputDirectory / "expr.hpp";

  WriteHeader(outputPath, expressionTypes);

  std::cout << "Generated " << outputPath.string() << '\n';

  return 0;
}