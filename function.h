#ifndef _FUNCTION_H_
#define _FUNCTION_H_
#include "bullet_train.h"
#include "context.h"
#include "value.h"

/* VM instructions */
enum {
    OP_PUSH, OP_PUSHBOOL, OP_PUSHNIL,
    OP_CLOSURE,
    OP_STORE, OP_LOAD,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_NEG, OP_NOT,
    OP_EQUAL, OP_LESS, OP_LEQUAL,
    OP_AND, OP_OR,
    OP_JUMP, OP_JUMPIF,
    OP_CALL,
    OP_RETURN,
    OP_PRINT
};

/*
** Instructions are 32 bits long.
** 8 bits for the opcode, 16 for arg A, 8 for arg B.
** |      argA      |  argB  | opcode |
*/
typedef unsigned long Instruction;

/* Function types */
typedef enum {
    FT_FUNC,
    FT_TASK,
    FT_GEN
} FType;

/* Data not small enough to fit in an instruction */
typedef union {
    bt_Value value;
    Key* key;
    bt_Function* function;
} FData;

/*
** Information about a function.
** Not callable on its own -- needs to be part of a bt_Closure
** so that variables in a higher scope are available.
*/
struct bt_Function {
    Instruction* program;
    FData* data;
    bt_Function* next; // Linked list of children (and their children)
    int params; // Number of parameters
    int locals; // Number of local variables (including parameters)
    FType type; // Type of function (func, task, or gen)
};

#endif