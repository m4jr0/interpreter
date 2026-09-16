#include "parser.hpp"

#include <memory>
#include <utility>

#include "lox.hpp"

namespace jlox {

std::vector<StmtPtr> Parser::Parse() {
  std::vector<StmtPtr> statements;

  while (!IsAtEnd()) {
    if (StmtPtr statement = Declaration()) {
      statements.push_back(std::move(statement));
    }
  }

  return statements;
}

ExprPtr Parser::ParseExpression() {
  try {
    ExprPtr expression = Expression();

    Consume(TokenType::Eof, "Expect end of expression.");

    return expression;
  } catch (const ParseError &) {
    return nullptr;
  }
}

StmtPtr Parser::Declaration() {
  try {
    if (Match({TokenType::Var})) {
      return VarDeclaration();
    }

    return Statement();
  } catch (const ParseError &) {
    Synchronize();
    return nullptr;
  }
}

StmtPtr Parser::VarDeclaration() {
  const Token &name = Consume(TokenType::Identifier, "Expect variable name.");

  ExprPtr initializer;

  if (Match({TokenType::Equal})) {
    initializer = Expression();
  }

  Consume(TokenType::Semicolon, "Expect ';' after variable declaration.");

  return std::make_unique<VarStmt>(name, std::move(initializer));
}

StmtPtr Parser::Statement() {
  if (Match({TokenType::Print})) {
    return PrintStatement();
  }

  if (Match({TokenType::LeftBrace})) {
    return std::make_unique<BlockStmt>(Block());
  }

  return ExpressionStatement();
}

StmtPtr Parser::PrintStatement() {
  ExprPtr value = Expression();

  Consume(TokenType::Semicolon, "Expect ';' after value.");

  return std::make_unique<PrintStmt>(std::move(value));
}

StmtPtr Parser::ExpressionStatement() {
  ExprPtr expression = Expression();

  Consume(TokenType::Semicolon, "Expect ';' after expression.");

  return std::make_unique<ExpressionStmt>(std::move(expression));
}

std::vector<StmtPtr> Parser::Block() {
  std::vector<StmtPtr> statements;

  while (!Check(TokenType::RightBrace) && !IsAtEnd()) {
    if (StmtPtr statement = Declaration()) {
      statements.push_back(std::move(statement));
    }
  }

  Consume(TokenType::RightBrace, "Expect '}' after block.");

  return statements;
}

ExprPtr Parser::Expression() { return Comma(); }

ExprPtr Parser::Comma() {
  ExprPtr expr = Assignment();

  while (Match({TokenType::Comma})) {
    const Token op = Previous();
    ExprPtr right = Assignment();

    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }

  return expr;
}

ExprPtr Parser::Assignment() {
  ExprPtr expr = Conditional();

  if (Match({TokenType::Equal})) {
    const Token equals = Previous();

    ExprPtr value = Assignment();

    if (auto *variable = dynamic_cast<VariableExpr *>(expr.get())) {
      const Token name = variable->name;

      return std::make_unique<AssignExpr>(name, std::move(value));
    }

    Error(equals, "Invalid assignment target.");
  }

  return expr;
}

ExprPtr Parser::Conditional() {
  ExprPtr expr = Equality();

  if (Match({TokenType::Question})) {
    ExprPtr thenBranch = Expression();

    Consume(TokenType::Colon, "Expect ':' after conditional expression.");

    ExprPtr elseBranch = Conditional();

    expr = std::make_unique<ConditionalExpr>(
        std::move(expr), std::move(thenBranch), std::move(elseBranch));
  }

  return expr;
}

ExprPtr Parser::Equality() {
  ExprPtr expr = Comparison();

  while (Match({
      TokenType::BangEqual,
      TokenType::EqualEqual,
  })) {
    Token op = Previous();
    ExprPtr right = Comparison();

    expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op),
                                        std::move(right));
  }

  return expr;
}

ExprPtr Parser::Comparison() {
  ExprPtr expr = Term();

  while (Match({
      TokenType::Greater,
      TokenType::GreaterEqual,
      TokenType::Less,
      TokenType::LessEqual,
  })) {
    Token op = Previous();
    ExprPtr right = Term();

    expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op),
                                        std::move(right));
  }

  return expr;
}

ExprPtr Parser::Term() {
  ExprPtr expr = Factor();

  while (Match({
      TokenType::Minus,
      TokenType::Plus,
  })) {
    Token op = Previous();
    ExprPtr right = Factor();

    expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op),
                                        std::move(right));
  }

  return expr;
}

ExprPtr Parser::Factor() {
  ExprPtr expr = Unary();

  while (Match({
      TokenType::Slash,
      TokenType::Star,
  })) {
    Token op = Previous();
    ExprPtr right = Unary();

    expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op),
                                        std::move(right));
  }

  return expr;
}

ExprPtr Parser::Unary() {
  if (Match({
          TokenType::Bang,
          TokenType::Minus,
      })) {
    Token op = Previous();
    ExprPtr right = Unary();

    return std::make_unique<UnaryExpr>(std::move(op), std::move(right));
  }

  if (Match({
          TokenType::Star,
          TokenType::Slash,
      })) {
    const Token op = Previous();

    Lox::Error(op, "Binary operator requires a left-hand operand.");

    Unary();
    throw ParseError{};
  }

  if (Match({
          TokenType::Plus,
      })) {
    const Token op = Previous();

    Lox::Error(op, "Binary operator requires a left-hand operand.");

    Factor();
    throw ParseError{};
  }

  if (Match({
          TokenType::Greater,
          TokenType::GreaterEqual,
          TokenType::Less,
          TokenType::LessEqual,
      })) {
    const Token op = Previous();

    Lox::Error(op, "Binary operator requires a left-hand operand.");

    Term();
    throw ParseError{};
  }

  if (Match({
          TokenType::BangEqual,
          TokenType::EqualEqual,
      })) {
    const Token op = Previous();

    Lox::Error(op, "Binary operator requires a left-hand operand.");

    Comparison();
    throw ParseError{};
  }

  if (Match({TokenType::Comma})) {
    const Token op = Previous();

    Lox::Error(op, "Binary operator requires a left-hand operand.");

    Conditional();
    throw ParseError{};
  }

  return Primary();
}

ExprPtr Parser::Primary() {
  if (Match({TokenType::False})) {
    return std::make_unique<LiteralExpr>(false);
  }

  if (Match({TokenType::True})) {
    return std::make_unique<LiteralExpr>(true);
  }

  if (Match({TokenType::Nil})) {
    return std::make_unique<LiteralExpr>(std::monostate{});
  }

  if (Match({
          TokenType::Number,
          TokenType::String,
      })) {
    return std::make_unique<LiteralExpr>(Previous().literal);
  }

  if (Match({TokenType::Identifier})) {
    return std::make_unique<VariableExpr>(Previous());
  }

  if (Match({TokenType::LeftParen})) {
    ExprPtr expr = Expression();

    Consume(TokenType::RightParen, "Expect ')' after expression.");

    return std::make_unique<GroupingExpr>(std::move(expr));
  }

  throw Error(Peek(), "Expect expression.");
}

bool Parser::Match(std::initializer_list<TokenType> types) {
  for (const TokenType type : types) {
    if (Check(type)) {
      Advance();
      return true;
    }
  }

  return false;
}

bool Parser::Check(TokenType type) const {
  if (IsAtEnd()) {
    return false;
  }

  return Peek().type == type;
}

const Token &Parser::Advance() {
  if (!IsAtEnd()) {
    ++current_;
  }

  return Previous();
}

bool Parser::IsAtEnd() const { return Peek().type == TokenType::Eof; }

const Token &Parser::Peek() const { return tokens_[current_]; }

const Token &Parser::Previous() const { return tokens_[current_ - 1]; }

const Token &Parser::Consume(TokenType type, std::string_view message) {
  if (Check(type)) {
    return Advance();
  }

  throw Error(Peek(), message);
}

Parser::ParseError Parser::Error(const Token &token, std::string_view message) {
  Lox::Error(token, message);
  return {};
}

void Parser::Synchronize() {
  Advance();

  while (!IsAtEnd()) {
    if (Previous().type == TokenType::Semicolon) {
      return;
    }

    switch (Peek().type) {
    case TokenType::Class:
    case TokenType::Fun:
    case TokenType::Var:
    case TokenType::For:
    case TokenType::If:
    case TokenType::While:
    case TokenType::Print:
    case TokenType::Return:
      return;

    default:
      break;
    }

    Advance();
  }
}

} // namespace jlox