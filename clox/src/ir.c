#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "value.h"

void initIRFunction(IRFunction *function) {
  function->instructions = NULL;
  function->instructionCount = 0;
  function->instructionCapacity = 0;
  function->blocks = NULL;
  function->blockCount = 0;
  function->blockCapacity = 0;
  function->entry = IR_NO_BLOCK;
  function->nextValue = 0;
}

void freeIRFunction(IRFunction *function) {
  for (int i = 0; i < function->instructionCount; i++) {
    free(function->instructions[i].captures);
    free(function->instructions[i].inputs);
  }
  for (int i = 0; i < function->blockCount; i++) {
    free(function->blocks[i].predecessors);
    free(function->blocks[i].successors);
    free(function->blocks[i].entryValues);
    free(function->blocks[i].parameters);
    if (function->blocks[i].incomingValues != NULL) {
      for (int p = 0; p < function->blocks[i].predecessorCount; p++) {
        free(function->blocks[i].incomingValues[p]);
      }
    }
    free(function->blocks[i].incomingValues);
  }
  free(function->instructions);
  free(function->blocks);
  initIRFunction(function);
}

static IRInstruction *appendInstruction(IRFunction *function, IROp op,
                                        int line) {
  if (function->instructionCount == function->instructionCapacity) {
    int oldCapacity = function->instructionCapacity;
    function->instructionCapacity = oldCapacity < 16 ? 16 : oldCapacity * 2;
    IRInstruction *instructions =
        realloc(function->instructions,
                sizeof(IRInstruction) * (size_t)function->instructionCapacity);
    if (instructions == NULL) exit(1);
    function->instructions = instructions;
  }
  IRInstruction *instruction =
      &function->instructions[function->instructionCount++];
  instruction->op = op;
  instruction->result = IR_NO_VALUE;
  instruction->inputs = NULL;
  instruction->inputCount = 0;
  instruction->operand = -1;
  instruction->operand2 = -1;
  instruction->captures = NULL;
  instruction->captureCount = 0;
  instruction->line = line;
  instruction->target = IR_NO_BLOCK;
  instruction->fallthrough = IR_NO_BLOCK;
  instruction->removed = false;
  return instruction;
}

int currentIRPosition(const IRFunction *function) {
  return function->instructionCount;
}

void emitIRInstruction(IRFunction *function, IROp op, int line) {
  appendInstruction(function, op, line);
}

void emitIROperand(IRFunction *function, IROp op, int operand, int line) {
  IRInstruction *instruction = appendInstruction(function, op, line);
  instruction->operand = operand;
}

void emitIRInvoke(IRFunction *function, IROp op, int name, int argumentCount,
                  int line) {
  IRInstruction *instruction = appendInstruction(function, op, line);
  instruction->operand = name;
  instruction->operand2 = argumentCount;
}

int emitIRJump(IRFunction *function, IROp op, int line) {
  appendInstruction(function, op, line);
  return function->instructionCount - 1;
}

void patchIRJump(IRFunction *function, int instruction, int target) {
  if (instruction < 0 || instruction >= function->instructionCount) return;
  IRInstruction *jump = &function->instructions[instruction];
  if (jump->op != IR_JUMP && jump->op != IR_BRANCH) return;
  jump->target = target;
}

void emitIRClosure(IRFunction *function, int constant,
                   const IRUpvalueCapture *captures, int captureCount,
                   int line) {
  IRInstruction *instruction = appendInstruction(function, IR_CLOSURE, line);
  instruction->operand = constant;
  instruction->captureCount = captureCount;
  if (captureCount == 0) return;
  instruction->captures =
      malloc(sizeof(IRUpvalueCapture) * (size_t)captureCount);
  if (instruction->captures == NULL) exit(1);
  memcpy(instruction->captures, captures,
         sizeof(IRUpvalueCapture) * (size_t)captureCount);
}

static IRBlockId createBlock(IRFunction *function, int firstInstruction) {
  if (function->blockCount == function->blockCapacity) {
    int oldCapacity = function->blockCapacity;
    function->blockCapacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    IRBlock *blocks =
        realloc(function->blocks, sizeof(IRBlock) * function->blockCapacity);
    if (blocks == NULL) exit(1);
    function->blocks = blocks;
  }
  IRBlockId id = function->blockCount++;
  IRBlock *block = &function->blocks[id];
  block->id = id;
  block->firstInstruction = firstInstruction;
  block->instructionCount = 0;
  block->predecessors = NULL;
  block->predecessorCount = 0;
  block->predecessorCapacity = 0;
  block->successors = NULL;
  block->successorCount = 0;
  block->successorCapacity = 0;
  block->entryValues = NULL;
  block->parameters = NULL;
  block->incomingValues = NULL;
  block->stackDepth = -1;
  if (function->entry == IR_NO_BLOCK) function->entry = id;
  return id;
}

void sealIRFunction(IRFunction *function) {
  if (function->instructionCount == 0) return;

  bool *leaders =
      calloc((size_t)function->instructionCount + 1, sizeof(bool));
  int *instructionToBlock =
      malloc(sizeof(int) * ((size_t)function->instructionCount + 1));
  if (leaders == NULL || instructionToBlock == NULL) exit(1);
  for (int i = 0; i <= function->instructionCount; i++) {
    instructionToBlock[i] = IR_NO_BLOCK;
  }
  leaders[0] = true;

  for (int i = 0; i < function->instructionCount; i++) {
    IRInstruction *instruction = &function->instructions[i];
    if (instruction->op == IR_JUMP || instruction->op == IR_BRANCH) {
      if (instruction->target >= 0 &&
          instruction->target < function->instructionCount) {
        leaders[instruction->target] = true;
      }
      if (i + 1 < function->instructionCount) leaders[i + 1] = true;
    } else if (instruction->op == IR_RETURN &&
               i + 1 < function->instructionCount) {
      leaders[i + 1] = true;
    }
  }

  IRBlockId currentBlock = IR_NO_BLOCK;
  for (int i = 0; i < function->instructionCount; i++) {
    if (leaders[i]) {
      currentBlock = createBlock(function, i);
      instructionToBlock[i] = currentBlock;
    }
    function->blocks[currentBlock].instructionCount++;
  }

  for (int i = 0; i < function->instructionCount; i++) {
    IRInstruction *instruction = &function->instructions[i];
    if (instruction->op != IR_JUMP && instruction->op != IR_BRANCH) continue;
    if (instruction->target >= 0 &&
        instruction->target < function->instructionCount) {
      instruction->target = instructionToBlock[instruction->target];
    } else {
      instruction->target = IR_NO_BLOCK;
    }
    if (instruction->op == IR_BRANCH && i + 1 < function->instructionCount) {
      instruction->fallthrough = instructionToBlock[i + 1];
    }
  }

  free(leaders);
  free(instructionToBlock);
  rebuildIRControlFlow(function);
}

static void addBlockId(IRBlockId **values, int *count, int *capacity,
                       IRBlockId value) {
  if (value == IR_NO_BLOCK) return;
  for (int i = 0; i < *count; i++) {
    if ((*values)[i] == value) return;
  }
  if (*count == *capacity) {
    int oldCapacity = *capacity;
    *capacity = oldCapacity < 4 ? 4 : oldCapacity * 2;
    IRBlockId *newValues = realloc(*values, sizeof(IRBlockId) * *capacity);
    if (newValues == NULL) exit(1);
    *values = newValues;
  }
  (*values)[(*count)++] = value;
}

static void addEdge(IRFunction *function, IRBlockId from, IRBlockId to) {
  if (to == IR_NO_BLOCK) return;
  IRBlock *source = &function->blocks[from];
  IRBlock *destination = &function->blocks[to];
  addBlockId(&source->successors, &source->successorCount,
             &source->successorCapacity, to);
  addBlockId(&destination->predecessors, &destination->predecessorCount,
             &destination->predecessorCapacity, from);
}

void rebuildIRControlFlow(IRFunction *function) {
  for (int i = 0; i < function->blockCount; i++) {
    function->blocks[i].predecessorCount = 0;
    function->blocks[i].successorCount = 0;
  }
  for (int i = 0; i < function->blockCount; i++) {
    IRBlock *block = &function->blocks[i];
    if (block->instructionCount == 0) continue;
    IRInstruction *last =
        &function->instructions[block->firstInstruction +
                                block->instructionCount - 1];
    if (last->op == IR_JUMP) {
      addEdge(function, i, last->target);
    } else if (last->op == IR_BRANCH) {
      addEdge(function, i, last->target);
      addEdge(function, i, last->fallthrough);
    } else if (last->op != IR_RETURN && i + 1 < function->blockCount) {
      addEdge(function, i, i + 1);
    }
  }
}


static IRValue newValue(IRFunction *function) {
  return function->nextValue++;
}

static void setInputs(IRInstruction *instruction, const IRValue *values,
                      int count) {
  if (instruction->inputCount != count) {
    IRValue *inputs = count > 0 ? realloc(instruction->inputs,
                                          sizeof(IRValue) * (size_t)count)
                                : NULL;
    if (count > 0 && inputs == NULL) exit(1);
    if (count == 0) free(instruction->inputs);
    instruction->inputs = inputs;
    instruction->inputCount = count;
  }
  for (int i = 0; i < count; i++) instruction->inputs[i] = values[i];
}

static IRValue ensureResult(IRFunction *function, IRInstruction *instruction) {
  if (instruction->result == IR_NO_VALUE) {
    instruction->result = newValue(function);
  }
  return instruction->result;
}

static bool popValues(IRValue *stack, int *depth, IRValue *values, int count) {
  if (*depth < count) return false;
  int first = *depth - count;
  for (int i = 0; i < count; i++) values[i] = stack[first + i];
  *depth -= count;
  return true;
}

static bool pushValue(IRValue **stack, int *depth, int *capacity,
                      IRValue value) {
  if (*depth == *capacity) {
    int oldCapacity = *capacity;
    *capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    IRValue *values = realloc(*stack, sizeof(IRValue) * (size_t)*capacity);
    if (values == NULL) return false;
    *stack = values;
  }
  (*stack)[(*depth)++] = value;
  return true;
}

static bool initializeBlockValues(IRFunction *function, IRBlock *block,
                                  int depth) {
  if (block->stackDepth >= 0) return block->stackDepth == depth;

  block->stackDepth = depth;
  if (depth > 0) {
    block->entryValues = malloc(sizeof(IRValue) * (size_t)depth);
    block->parameters = malloc(sizeof(IRValue) * (size_t)depth);
    if (block->entryValues == NULL || block->parameters == NULL) exit(1);
    for (int i = 0; i < depth; i++) {
      IRValue parameter = newValue(function);
      block->entryValues[i] = parameter;
      block->parameters[i] = parameter;
    }
  }

  if (block->predecessorCount > 0) {
    block->incomingValues =
        calloc((size_t)block->predecessorCount, sizeof(IRValue *));
    if (block->incomingValues == NULL) exit(1);
  }
  return true;
}

static int predecessorIndex(const IRBlock *block, IRBlockId predecessor) {
  for (int i = 0; i < block->predecessorCount; i++) {
    if (block->predecessors[i] == predecessor) return i;
  }
  return -1;
}

static bool recordIncomingValues(IRBlock *block, IRBlockId predecessor,
                                 const IRValue *values, int depth) {
  if (block->stackDepth != depth) return false;
  int index = predecessorIndex(block, predecessor);
  if (index < 0) return false;

  IRValue *incoming = block->incomingValues[index];
  if (incoming == NULL && depth > 0) {
    incoming = malloc(sizeof(IRValue) * (size_t)depth);
    if (incoming == NULL) exit(1);
    block->incomingValues[index] = incoming;
  }
  for (int i = 0; i < depth; i++) incoming[i] = values[i];
  return true;
}

static bool simulateInstruction(IRFunction *function,
                                IRInstruction *instruction, IRValue **stack,
                                int *depth, int *capacity) {
  IRValue values[2];
  switch (instruction->op) {
  case IR_CONSTANT:
  case IR_NIL:
  case IR_TRUE:
  case IR_FALSE:
  case IR_LOAD_GLOBAL:
  case IR_LOAD_UPVALUE:
  case IR_CLASS:
  case IR_CLOSURE:
    setInputs(instruction, NULL, 0);
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));

  case IR_LOAD_LOCAL:
    if (instruction->operand < 0 || instruction->operand >= *depth) return false;
    values[0] = (*stack)[instruction->operand];
    setInputs(instruction, values, 1);
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));

  case IR_STORE_LOCAL:
    if (*depth < 1 || instruction->operand < 0 ||
        instruction->operand >= *depth) {
      return false;
    }
    values[0] = (*stack)[*depth - 1];
    setInputs(instruction, values, 1);
    (*stack)[instruction->operand] = values[0];
    return true;

  case IR_STORE_GLOBAL:
  case IR_STORE_UPVALUE:
    if (*depth < 1) return false;
    values[0] = (*stack)[*depth - 1];
    setInputs(instruction, values, 1);
    return true;

  case IR_DEFINE_GLOBAL:
  case IR_POP:
  case IR_PRINT:
  case IR_CLOSE_UPVALUE:
  case IR_RETURN:
    if (!popValues(*stack, depth, values, 1)) return false;
    setInputs(instruction, values, 1);
    return true;

  case IR_EQUAL:
  case IR_GREATER:
  case IR_LESS:
  case IR_ADD:
  case IR_SUBTRACT:
  case IR_MULTIPLY:
  case IR_DIVIDE:
    if (!popValues(*stack, depth, values, 2)) return false;
    setInputs(instruction, values, 2);
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));

  case IR_NOT:
  case IR_NEGATE:
  case IR_GET_PROPERTY:
    if (!popValues(*stack, depth, values, 1)) return false;
    setInputs(instruction, values, 1);
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));

  case IR_SET_PROPERTY:
    if (!popValues(*stack, depth, values, 2)) return false;
    setInputs(instruction, values, 2);
    return pushValue(stack, depth, capacity, values[1]);

  case IR_GET_SUPER:
    if (!popValues(*stack, depth, values, 2)) return false;
    setInputs(instruction, values, 2);
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));

  case IR_CALL: {
    int count = instruction->operand + 1;
    if (*depth < count) return false;
    setInputs(instruction, *stack + *depth - count, count);
    *depth -= count;
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));
  }

  case IR_INVOKE: {
    int count = instruction->operand2 + 1;
    if (*depth < count) return false;
    setInputs(instruction, *stack + *depth - count, count);
    *depth -= count;
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));
  }

  case IR_SUPER_INVOKE: {
    int count = instruction->operand2 + 2;
    if (*depth < count) return false;
    setInputs(instruction, *stack + *depth - count, count);
    *depth -= count;
    return pushValue(stack, depth, capacity,
                     ensureResult(function, instruction));
  }

  case IR_INHERIT:
    if (!popValues(*stack, depth, values, 2)) return false;
    setInputs(instruction, values, 2);
    return pushValue(stack, depth, capacity, values[0]);

  case IR_METHOD:
    if (!popValues(*stack, depth, values, 2)) return false;
    setInputs(instruction, values, 2);
    return pushValue(stack, depth, capacity, values[0]);

  case IR_BRANCH:
    if (*depth < 1) return false;
    values[0] = (*stack)[*depth - 1];
    setInputs(instruction, values, 1);
    return true;

  case IR_JUMP:
    setInputs(instruction, NULL, 0);
    return true;
  }
  return false;
}

bool materializeIRValues(IRFunction *function, int initialStackDepth) {
  if (function->blockCount == 0) return true;
  if (initialStackDepth < 0 || function->entry == IR_NO_BLOCK) return false;

  for (int i = 0; i < function->instructionCount; i++) {
    function->instructions[i].result = IR_NO_VALUE;
    setInputs(&function->instructions[i], NULL, 0);
  }
  function->nextValue = 0;

  IRBlock *entry = &function->blocks[function->entry];
  if (!initializeBlockValues(function, entry, initialStackDepth)) return false;

  bool *queued = calloc((size_t)function->blockCount, sizeof(bool));
  bool *processed = calloc((size_t)function->blockCount, sizeof(bool));
  IRBlockId *queue =
      malloc(sizeof(IRBlockId) * (size_t)function->blockCount);
  if (queued == NULL || processed == NULL || queue == NULL) exit(1);

  int head = 0;
  int tail = 0;
  queue[tail++] = function->entry;
  queued[function->entry] = true;

  while (head < tail) {
    IRBlockId blockId = queue[head++];
    queued[blockId] = false;
    if (processed[blockId]) continue;
    processed[blockId] = true;

    IRBlock *block = &function->blocks[blockId];
    int depth = block->stackDepth;
    int capacity = depth < 8 ? 8 : depth;
    IRValue *stack = malloc(sizeof(IRValue) * (size_t)capacity);
    if (stack == NULL) exit(1);
    for (int i = 0; i < depth; i++) stack[i] = block->entryValues[i];

    for (int i = 0; i < block->instructionCount; i++) {
      IRInstruction *instruction =
          &function->instructions[block->firstInstruction + i];
      if (!simulateInstruction(function, instruction, &stack, &depth,
                               &capacity)) {
        free(stack);
        free(queued);
        free(processed);
        free(queue);
        return false;
      }
    }

    for (int i = 0; i < block->successorCount; i++) {
      IRBlockId successorId = block->successors[i];
      IRBlock *successor = &function->blocks[successorId];
      if (!initializeBlockValues(function, successor, depth) ||
          !recordIncomingValues(successor, blockId, stack, depth)) {
        free(stack);
        free(queued);
        free(processed);
        free(queue);
        return false;
      }
      if (!processed[successorId] && !queued[successorId]) {
        queue[tail++] = successorId;
        queued[successorId] = true;
      }
    }
    free(stack);
  }

  free(queued);
  free(processed);
  free(queue);
  return true;
}

static const char *opName(IROp op) {
  static const char *names[] = {
      "constant",       "nil",          "true",         "false",
      "pop",            "load_local",   "store_local",  "load_global",
      "define_global",  "store_global", "load_upvalue", "store_upvalue",
      "get_property",   "set_property", "get_super",    "invoke",
      "super_invoke",   "equal",        "greater",      "less",
      "add",            "subtract",     "multiply",     "divide",
      "not",            "negate",       "print",        "jump",
      "branch",         "call",         "closure",      "close_upvalue",
      "return",         "class",        "inherit",      "method"};
  int index = (int)op;
  int count = (int)(sizeof(names) / sizeof(names[0]));
  return index >= 0 && index < count ? names[index] : "unknown";
}

static bool hasSingleOperand(IROp op) {
  switch (op) {
  case IR_CONSTANT:
  case IR_LOAD_LOCAL:
  case IR_STORE_LOCAL:
  case IR_LOAD_GLOBAL:
  case IR_DEFINE_GLOBAL:
  case IR_STORE_GLOBAL:
  case IR_LOAD_UPVALUE:
  case IR_STORE_UPVALUE:
  case IR_GET_PROPERTY:
  case IR_SET_PROPERTY:
  case IR_GET_SUPER:
  case IR_CALL:
  case IR_CLASS:
  case IR_METHOD:
    return true;
  default:
    return false;
  }
}

static void printBlockTarget(const IRFunction *function,
                             IRBlockId predecessor, IRBlockId target) {
  printf("block%d", target);
  if (target < 0 || target >= function->blockCount) return;

  const IRBlock *block = &function->blocks[target];
  int index = predecessorIndex(block, predecessor);
  if (index < 0 || block->stackDepth <= 0 ||
      block->incomingValues == NULL ||
      block->incomingValues[index] == NULL) {
    return;
  }

  printf("(");
  for (int i = 0; i < block->stackDepth; i++) {
    if (i > 0) printf(", ");
    printf("%%%d", block->incomingValues[index][i]);
  }
  printf(")");
}

void printIRFunction(const IRFunction *function,
                     const ValueArray *constants, const char *name) {
  printf("== IR %s ==\n", name != NULL ? name : "<script>");
  for (int i = 0; i < function->blockCount; i++) {
    const IRBlock *block = &function->blocks[i];
    printf("block%d", block->id);
    if (block->stackDepth > 0 && block->parameters != NULL) {
      printf("(");
      for (int s = 0; s < block->stackDepth; s++) {
        if (s > 0) printf(", ");
        printf("%%%d", block->parameters[s]);
      }
      printf(")");
    }
    if (block->predecessorCount > 0) {
      printf(" ; preds:");
      for (int p = 0; p < block->predecessorCount; p++) {
        printf(" block%d", block->predecessors[p]);
      }
    }
    printf("\n");

    for (int j = 0; j < block->instructionCount; j++) {
      const IRInstruction *instruction =
          &function->instructions[block->firstInstruction + j];
      printf("  %s", instruction->removed ? "; removed " : "");
      if (instruction->result != IR_NO_VALUE) {
        printf("%%%d = ", instruction->result);
      }
      printf("%s", opName(instruction->op));
      if (instruction->inputCount > 0) {
        printf(" ");
        for (int v = 0; v < instruction->inputCount; v++) {
          if (v > 0) printf(", ");
          printf("%%%d", instruction->inputs[v]);
        }
      }
      if (instruction->op == IR_JUMP) {
        printf(" ");
        printBlockTarget(function, block->id, instruction->target);
      } else if (instruction->op == IR_BRANCH) {
        printf(" ");
        printBlockTarget(function, block->id, instruction->fallthrough);
        printf(", ");
        printBlockTarget(function, block->id, instruction->target);
      } else if (instruction->op == IR_INVOKE ||
                 instruction->op == IR_SUPER_INVOKE) {
        printf(" %d, argc %d", instruction->operand, instruction->operand2);
      } else if (instruction->op == IR_CLOSURE) {
        printf(" %d", instruction->operand);
        for (int capture = 0; capture < instruction->captureCount; capture++) {
          printf(" [%s %d]",
                 instruction->captures[capture].isLocal ? "local" : "upvalue",
                 instruction->captures[capture].index);
        }
      } else if (hasSingleOperand(instruction->op)) {
        printf(" %d", instruction->operand);
      }

      if (instruction->op == IR_CONSTANT && instruction->operand >= 0 &&
          instruction->operand < constants->count) {
        printf(" ; ");
        printValue(constants->values[instruction->operand]);
      }
      printf("\n");
    }
    printf("\n");
  }
}
