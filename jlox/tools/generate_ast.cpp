#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
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

std::string LowerFirst(std::string value) {
  if (!value.empty()) {
    value[0] =
        static_cast<char>(std::tolower(static_cast<unsigned char>(value[0])));
  }

  return value;
}

void DefineAst(const std::filesystem::path &outputDir,
               std::string_view baseName, const std::vector<AstType> &types) {
  const std::string filename =
      std::string(baseName) == "Expr" ? "expr.hpp" : "stmt.hpp";

  const auto path = outputDir / filename;
  std::ofstream out(path);

  if (!out) {
    throw std::runtime_error("Could not open " + path.string());
  }

  out << "#pragma once\n\n";

  out << "#include \"token.hpp\"\n";

  if (baseName == "Expr") {
    out << "#include \"value.hpp\"\n";
  }

  if (baseName == "Stmt") {
    out << "#include \"expr.hpp\"\n";
  }

  out << "\n";
  out << "#include <memory>\n";
  out << "#include <utility>\n";
  out << "#include <vector>\n\n";

  out << "namespace jlox {\n\n";

  out << "class " << baseName << ";\n";

  for (const auto &type : types) {
    out << "class " << type.name << baseName << ";\n";
  }

  out << "\n";

  out << "class " << baseName << "Visitor {\n";
  out << "public:\n";
  out << "  virtual ~" << baseName << "Visitor() = default;\n\n";

  for (const auto &type : types) {
    out << "  virtual void Visit" << type.name << baseName << "(const "
        << type.name << baseName << " &" << LowerFirst(type.name) << ") = 0;\n";
  }

  out << "};\n\n";

  out << "class " << baseName << " {\n";
  out << "public:\n";
  out << "  virtual ~" << baseName << "() = default;\n";
  out << "  virtual void Accept(" << baseName
      << "Visitor &visitor) const = 0;\n";
  out << "};\n\n";

  out << "using " << baseName << "Ptr = std::unique_ptr<" << baseName
      << ">;\n\n";

  for (const auto &type : types) {
    const std::string className = type.name + std::string(baseName);

    out << "class " << className << " final : public " << baseName << " {\n";
    out << "public:\n";

    out << "  " << className << "(";

    for (std::size_t i = 0; i < type.fields.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }

      out << type.fields[i].type << " " << type.fields[i].name;
    }

    out << ")\n      : ";

    for (std::size_t i = 0; i < type.fields.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }

      out << type.fields[i].name << "(std::move(" << type.fields[i].name
          << "))";
    }

    out << " {}\n\n";

    out << "  void Accept(" << baseName
        << "Visitor &visitor) const override {\n";
    out << "    visitor.Visit" << type.name << baseName << "(*this);\n";
    out << "  }\n\n";

    for (const auto &field : type.fields) {
      out << "  " << field.type << " " << field.name << ";\n";
    }

    out << "};\n\n";
  }

  out << "} // namespace jlox\n";
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: generate_ast <output directory>\n";
    return 64;
  }

  const std::filesystem::path outputDir = argv[1];

  const std::vector<AstType> expressionTypes{
      {"Assign",
       {
           {"Token", "name"},
           {"ExprPtr", "value"},
       }},
      {"Binary",
       {
           {"ExprPtr", "left"},
           {"Token", "op"},
           {"ExprPtr", "right"},
       }},
      {"Conditional",
       {
           {"ExprPtr", "condition"},
           {"ExprPtr", "thenBranch"},
           {"ExprPtr", "elseBranch"},
       }},
      {"Grouping",
       {
           {"ExprPtr", "expression"},
       }},
      {"Literal",
       {
           {"Literal", "value"},
       }},
      {"Unary",
       {
           {"Token", "op"},
           {"ExprPtr", "right"},
       }},
      {"Variable",
       {
           {"Token", "name"},
       }},
  };

  const std::vector<AstType> statementTypes{
      {"Block",
       {
           {"std::vector<StmtPtr>", "statements"},
       }},
      {"Expression",
       {
           {"ExprPtr", "expression"},
       }},
      {"Print",
       {
           {"ExprPtr", "expression"},
       }},
      {"Var",
       {
           {"Token", "name"},
           {"ExprPtr", "initializer"},
       }},
  };

  DefineAst(outputDir, "Expr", expressionTypes);
  DefineAst(outputDir, "Stmt", statementTypes);

  return 0;
}