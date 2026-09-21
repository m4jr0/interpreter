#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "ir.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

// -----------------------------------------------------------------------------
// Compiler types.
// -----------------------------------------------------------------------------

typedef struct {
  Token current;
  Token previous;

  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;
  IRFunction ir;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler *enclosing;
  bool hasSuperclass;
} ClassCompiler;

// -----------------------------------------------------------------------------
// Compiler state.
// -----------------------------------------------------------------------------

Parser parser;
Compiler *current = NULL;
ClassCompiler *currentClass = NULL;

// -----------------------------------------------------------------------------
// Forward declarations.
// -----------------------------------------------------------------------------

static void error(const char *message);

static void varDeclaration(void);
static void classDeclaration(void);
static void declaration(void);
static void statement(void);
static void expression(void);

static void ifStatement(void);
static void whileStatement(void);
static void forStatement(void);
static void returnStatement(void);

static void parsePrecedence(Precedence precedence);
static ParseRule *getRule(TokenType type);

static void variable(bool canAssign);
static uint8_t argumentList(void);

// -----------------------------------------------------------------------------
// IR emission.
// -----------------------------------------------------------------------------

static Chunk *currentChunk(void) { return &current->function->chunk; }

static void emitOp(IROp op) {
  emitIRInstruction(&current->ir, op, parser.previous.line);
}

static void emitOperand(IROp op, int operand) {
  emitIROperand(&current->ir, op, operand, parser.previous.line);
}

static int emitJump(IROp op) {
  return emitIRJump(&current->ir, op, parser.previous.line);
}

static void patchJump(int instruction) {
  patchIRJump(&current->ir, instruction, currentIRPosition(&current->ir));
}

static void emitLoop(int loopStart) {
  int jump = emitIRJump(&current->ir, IR_JUMP, parser.previous.line);
  patchIRJump(&current->ir, jump, loopStart);
}

static void emitReturn(void) {
  if (current->type == TYPE_INITIALIZER) emitOperand(IR_LOAD_LOCAL, 0);
  else emitOp(IR_NIL);
  emitOp(IR_RETURN);
}

// -----------------------------------------------------------------------------
// Error handling.
// -----------------------------------------------------------------------------

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode) {
    return;
  }

  parser.panicMode = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

// -----------------------------------------------------------------------------
// Token handling.
// -----------------------------------------------------------------------------

static void advance(void) {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();

    if (parser.current.type != TOKEN_ERROR) {
      break;
    }

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  }

  advance();
  return true;
}

static void synchronize(void) {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) {
      return;
    }

    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;

    default:
      break;
    }

    advance();
  }
}

// -----------------------------------------------------------------------------
// Constants.
// -----------------------------------------------------------------------------

static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);

  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitOperand(IR_CONSTANT, makeConstant(value));
}

static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

// -----------------------------------------------------------------------------
// Compiler lifecycle.
// -----------------------------------------------------------------------------

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  initIRFunction(&compiler->ir);

  compiler->function = newFunction();
  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name =
        copyString(parser.previous.start, parser.previous.length);
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;

  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

static ObjFunction *endCompiler(void) {
  emitReturn();

  ObjFunction *function = current->function;

  sealIRFunction(&current->ir);
  if (!parser.hadError &&
      !buildIRValues(&current->ir, function->arity + 1)) {
    error("Failed to build explicit IR values.");
  }

#ifdef DEBUG_PRINT_IR
  if (!parser.hadError) {
    printIRFunction(&current->ir, currentChunk(),
                    function->name != NULL ? function->name->chars
                                           : "<script>");
  }
#endif

  if (!parser.hadError) {
    Chunk lowered;
    if (!lowerIRToChunk(&current->ir, currentChunk(), &lowered)) {
      error("Failed to lower IR.");
    } else {
      freeChunk(currentChunk());
      function->chunk = lowered;
    }
  }

  freeIRFunction(&current->ir);

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif

  current = current->enclosing;
  return function;
}

// -----------------------------------------------------------------------------
// Local variables and lexical scopes.
// -----------------------------------------------------------------------------

static void beginScope(void) { current->scopeDepth++; }

static void endScope(void) {
  current->scopeDepth--;

  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      emitOp(IR_CLOSE_UPVALUE);
    } else {
      emitOp(IR_POP);
    }

    current->localCount--;
  }
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length) {
    return false;
  }

  return memcmp(a->start, b->start, a->length) == 0;
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVariable(void) {
  if (current->scopeDepth == 0) {
    return;
  }

  Token *name = &parser.previous;

  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];

    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

static void markInitialized(void) {
  if (current->scopeDepth == 0) {
    return;
  }

  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];

    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }

      return i;
    }
  }

  return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];

    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;

  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL)
    return -1;

  int local = resolveLocal(compiler->enclosing, name);

  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);

  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

// -----------------------------------------------------------------------------
// Variable declarations.
// -----------------------------------------------------------------------------

static uint8_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable();

  if (current->scopeDepth > 0) {
    return 0;
  }

  return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitOperand(IR_DEFINE_GLOBAL, global);
}

// -----------------------------------------------------------------------------
// Pratt parse functions.
// -----------------------------------------------------------------------------

static void number(bool canAssign) {
  (void)canAssign;

  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
  (void)canAssign;

  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void literal(bool canAssign) {
  (void)canAssign;

  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitOp(IR_FALSE);
    break;

  case TOKEN_NIL:
    emitOp(IR_NIL);
    break;

  case TOKEN_TRUE:
    emitOp(IR_TRUE);
    break;

  default:
    return;
  }
}

static void grouping(bool canAssign) {
  (void)canAssign;

  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign) {
  (void)canAssign;

  TokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_BANG:
    emitOp(IR_NOT);
    break;

  case TOKEN_MINUS:
    emitOp(IR_NEGATE);
    break;

  default:
    return;
  }
}

static void namedVariable(Token name, bool canAssign) {
  IROp getOp;
  IROp setOp;

  int arg = resolveLocal(current, &name);

  if (arg != -1) {
    getOp = IR_LOAD_LOCAL;
    setOp = IR_STORE_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = IR_LOAD_UPVALUE;
    setOp = IR_STORE_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = IR_LOAD_GLOBAL;
    setOp = IR_STORE_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitOperand(setOp, arg);
  } else {
    emitOperand(getOp, arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char *text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void super_(bool canAssign) {
  (void)canAssign;

  if (currentClass == NULL) {
    error("Can't use 'super' outside of a class.");
  } else if (!currentClass->hasSuperclass) {
    error("Can't use 'super' in a class with no superclass.");
  }

  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint8_t name = identifierConstant(&parser.previous);

  namedVariable(syntheticToken("this"), false);

  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitIRInvoke(&current->ir, IR_SUPER_INVOKE, name, argCount,
                 parser.previous.line);
  } else {
    namedVariable(syntheticToken("super"), false);
    emitOperand(IR_GET_SUPER, name);
  }
}

static void and_(bool canAssign) {
  (void)canAssign;

  int endJump = emitJump(IR_BRANCH);

  emitOp(IR_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

static void or_(bool canAssign) {
  (void)canAssign;

  int elseJump = emitJump(IR_BRANCH);
  int endJump = emitJump(IR_JUMP);

  patchJump(elseJump);

  emitOp(IR_POP);
  parsePrecedence(PREC_OR);

  patchJump(endJump);
}

static void this_(bool canAssign) {
  if (currentClass == NULL) {
    error("Can't use 'this' outside of a class.");
    return;
  }

  variable(false);
}

static uint8_t argumentList(void) {
  uint8_t argCount = 0;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();

      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }

      argCount++;
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}

static void call(bool canAssign) {
  (void)canAssign;

  uint8_t argCount = argumentList();
  emitOperand(IR_CALL, argCount);
}

static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitOperand(IR_SET_PROPERTY, name);
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    emitIRInvoke(&current->ir, IR_INVOKE, name, argCount,
                 parser.previous.line);
  } else {
    emitOperand(IR_GET_PROPERTY, name);
  }
}

static void binary(bool canAssign) {
  (void)canAssign;

  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);

  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitOp(IR_EQUAL);
    emitOp(IR_NOT);
    break;

  case TOKEN_EQUAL_EQUAL:
    emitOp(IR_EQUAL);
    break;

  case TOKEN_GREATER:
    emitOp(IR_GREATER);
    break;

  case TOKEN_GREATER_EQUAL:
    emitOp(IR_LESS);
    emitOp(IR_NOT);
    break;

  case TOKEN_LESS:
    emitOp(IR_LESS);
    break;

  case TOKEN_LESS_EQUAL:
    emitOp(IR_GREATER);
    emitOp(IR_NOT);
    break;

  case TOKEN_PLUS:
    emitOp(IR_ADD);
    break;

  case TOKEN_MINUS:
    emitOp(IR_SUBTRACT);
    break;

  case TOKEN_STAR:
    emitOp(IR_MULTIPLY);
    break;

  case TOKEN_SLASH:
    emitOp(IR_DIVIDE);
    break;

  default:
    return;
  }
}

// -----------------------------------------------------------------------------
// Pratt parse table.
// -----------------------------------------------------------------------------

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},

    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},

    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},

    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},

    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},

    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},

    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

// -----------------------------------------------------------------------------
// Pratt parser.
// -----------------------------------------------------------------------------

static void parsePrecedence(Precedence precedence) {
  advance();

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();

    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static void expression(void) { parsePrecedence(PREC_ASSIGNMENT); }

// -----------------------------------------------------------------------------
// Statements.
// -----------------------------------------------------------------------------

static void expressionStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitOp(IR_POP);
}

static void printStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitOp(IR_PRINT);
}

static void block(void) {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void ifStatement(void) {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(IR_BRANCH);
  emitOp(IR_POP);

  statement();

  int elseJump = emitJump(IR_JUMP);

  patchJump(thenJump);
  emitOp(IR_POP);

  if (match(TOKEN_ELSE)) {
    statement();
  }

  patchJump(elseJump);
}

static void whileStatement(void) {
  int loopStart = currentIRPosition(&current->ir);

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(IR_BRANCH);
  emitOp(IR_POP);

  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitOp(IR_POP);
}

static void forStatement(void) {
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentIRPosition(&current->ir);
  int exitJump = -1;

  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    exitJump = emitJump(IR_BRANCH);
    emitOp(IR_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(IR_JUMP);
    int incrementStart = currentIRPosition(&current->ir);

    expression();
    emitOp(IR_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;

    patchJump(bodyJump);
  }

  statement();
  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitOp(IR_POP);
  }

  endScope();
}

static void returnStatement(void) {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer.");
    }

    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitOp(IR_RETURN);
  }
}

static void statement(void) {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

// -----------------------------------------------------------------------------
// Functions and declarations.
// -----------------------------------------------------------------------------

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);

  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;

      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }

      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

  block();

  ObjFunction *function = endCompiler();
  IRUpvalueCapture captures[UINT8_COUNT];
  for (int i = 0; i < function->upvalueCount; i++) {
    captures[i].isLocal = compiler.upvalues[i].isLocal;
    captures[i].index = compiler.upvalues[i].index;
  }
  emitIRClosure(&current->ir, makeConstant(OBJ_VAL(function)), captures,
                function->upvalueCount, parser.previous.line);
}

static void method(void) {
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifierConstant(&parser.previous);

  FunctionType type = TYPE_METHOD;
  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }

  function(type);
  emitOperand(IR_METHOD, constant);
}

static void funDeclaration(void) {
  uint8_t global = parseVariable("Expect function name.");

  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void varDeclaration(void) {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitOp(IR_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  defineVariable(global);
}

static void classDeclaration(void) {
  consume(TOKEN_IDENTIFIER, "Expect class name.");
  Token className = parser.previous;
  uint8_t nameConstant = identifierConstant(&parser.previous);

  declareVariable();
  emitOperand(IR_CLASS, nameConstant);
  defineVariable(nameConstant);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;
  classCompiler.hasSuperclass = false;
  currentClass = &classCompiler;

  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);

    if (identifiersEqual(&className, &parser.previous)) {
      error("A class can't inherit from itself.");
    }

    beginScope();
    addLocal(syntheticToken("super"));
    defineVariable(0);

    namedVariable(className, false);
    emitOp(IR_INHERIT);
    classCompiler.hasSuperclass = true;
  }

  namedVariable(className, false);

  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  emitOp(IR_POP);

  if (classCompiler.hasSuperclass) {
    endScope();
  }

  currentClass = currentClass->enclosing;
}

static void declaration(void) {
  if (match(TOKEN_CLASS)) {
    classDeclaration();
  } else if (match(TOKEN_FUN)) {
    funDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode)
    synchronize();
}

// -----------------------------------------------------------------------------
// Public compiler entry point.
// -----------------------------------------------------------------------------

ObjFunction *compile(const char *source) {
  initScanner(source);

  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction *function = endCompiler();
  return parser.hadError ? NULL : function;
}

void markCompilerRoots(void) {
  Compiler *compiler = current;
  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}