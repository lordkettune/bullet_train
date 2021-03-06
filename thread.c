#include <stdlib.h>
#include <stdio.h>

#include "thread.h"
#include "function.h"
#include "context.h"
#include "struct.h"

/*
** Function call information.
** Basically just acts as a stack frame.
** Stored in a linked list so they can be reused, avoiding dynamic allocations.
*/
struct Call {
    Call* next;
    Call* previous;
    bt_Closure* closure;
    Instruction* ip;
    bt_Value* base;
};


bt_Thread* thread_new()
{
    bt_Thread* t = malloc(sizeof(bt_Thread));
    t->next = NULL;
    t->timer = 0;
    t->stack = malloc(sizeof(bt_Value) * 32);
    t->stacksize = 32;
    Call* c = malloc(sizeof(Call));
    c->previous = NULL;
    c->next = NULL;
    c->base = t->stack;
    t->call = c;
    return t;
}

/*
** Prints a bt_Value to stdout
*/
static void printvalue(bt_Value* vl)
{
    switch (vl->type)
    {
        case VT_NIL:    printf("nil\n"); break;
        case VT_NUMBER: printf("%g\n", vl->number); break;
        case VT_BOOL:   printf(vl->boolean ? "true\n" : "false\n"); break;
        default:        putchar('\n'); break;
    }
}

/*
** Test for equality of two bt_Values
*/
static int equal(bt_Value* l, bt_Value* r)
{
    if (l->type == r->type) // Have to be the same type to be equal
    {
        switch (l->type)
        {
            case VT_NIL:    return 1;
            case VT_BOOL:   return l->boolean == r->boolean;
            case VT_NUMBER: return l->number == r->number;
        }
    }
    return 0;
}

/*
** Less than comparison
** Errors if values are incompatible
*/
static int less(bt_Value* l, bt_Value* r)
{
    return l->number < r->number;   
}

/*
** Less than or equal comparision
** Also errors for incompatible types
*/
static int lequal(bt_Value* l, bt_Value* r)
{
    return l->number <= r->number;
}

/*
** Evaluates a bt_Value as a boolean
** nil - always false
** boolean - should be obvious :V
** number - false if 0
*/
static int test(bt_Value* vl)
{
    switch (vl->type)
    {
        case VT_BOOL:   return vl->boolean;
        case VT_NUMBER: return vl->number != 0;
        default:        return 0;
    }
}

/*
** ============================================================
** The interpreter, Bullet Train's heart and soul
** ============================================================
*/

// Shortcuts
#define arga(i) ((i >> 8) & 0xFF)
#define argb(i) ((i >> 16) & 0xFF)
#define argbx(i) (i >> 16)
#define argc(i) (i >> 24)

#define dest(i) reg[arga(i)]
#define rkb(i) (i & 0x40 ? &fn->constants[argb(i)] : &reg[argb(i)])
#define rkc(i) (i & 0x80 ? &fn->constants[argc(i)] : &reg[argc(i)])

#define number(n) ((bt_Value) { .number = (n), .type = VT_NUMBER })
#define boolean(b) ((bt_Value) { .boolean = (b), .type = VT_BOOL })
#define struc(b) ((bt_Value) { .struc = (b), .type = VT_STRUCT })

/*
** Main loop of the interpreter
*/
int thread_execute(bt_Context* bt, bt_Thread* t)
{
    Call* c;
    bt_Function* fn;
    bt_Value* reg;

// Refresh:
    c = t->call;
    reg = c->base;
    fn = c->closure->function;

    for (;;)
    {
        Instruction i = *c->ip++;
        switch (i & 0x3F)
        {
            case OP_LOAD: {
                dest(i) = fn->constants[argbx(i)];
                break;
            }
            case OP_LOADBOOL: {
                dest(i) = boolean(argb(i));
                c->ip += argc(i);
                break;
            }

            case OP_NEWSTRUCT: {
                dest(i) = struc(bt_newstruct(bt));
                break;
            }
            case OP_GETSTRUCT: {
                dest(i) = getstruct(reg[argb(i)].struc, fn->keys[argc(i)]);
                break;
            }
            case OP_SETSTRUCT: {
                setstruct(reg[arga(i)].struc, fn->keys[argb(i)], rkc(i));
                break;
            }

            case OP_MOVE: {
                dest(i) = reg[argbx(i)];
                break;
            }

            case OP_ADD: {
                bt_Value* lhs = rkb(i);
                bt_Value* rhs = rkc(i);
                dest(i) = number(lhs->number + rhs->number);
                break;
            }
            case OP_SUB: {
                bt_Value* lhs = rkb(i);
                bt_Value* rhs = rkc(i);
                dest(i) = number(lhs->number - rhs->number);
                break;
            }
            case OP_MUL: {
                bt_Value* lhs = rkb(i);
                bt_Value* rhs = rkc(i);
                dest(i) = number(lhs->number * rhs->number);
                break;
            }
            case OP_DIV: {
                bt_Value* lhs = rkb(i);
                bt_Value* rhs = rkc(i);
                dest(i) = number(lhs->number / rhs->number);
                break;
            }

            case OP_NEG: {
                bt_Value* vl = rkc(i);
                dest(i) = number(-vl->number);
                break;
            }
            case OP_NOT: {
                bt_Value* vl = rkc(i);
                dest(i) = boolean(!vl->boolean);
                break;
            }

            case OP_EQUAL: {
                if (equal(rkb(i), rkc(i)) == arga(i)) {
                    ++c->ip;
                }
                break;
            }
            case OP_LEQUAL: {
                if (lequal(rkb(i), rkc(i)) == arga(i)) {
                    ++c->ip;
                }
                break;
            }
            case OP_LESS: {
                if (less(rkb(i), rkc(i)) == arga(i)) {
                    ++c->ip;
                }
                break;
            }
            case OP_TEST: {
                if (test(rkc(i)) == arga(i)) {
                    ++c->ip;
                }
                break;
            }

            case OP_JUMP: {
                c->ip = fn->program + argbx(i);
                break;
            }

            case OP_RETURN: {
                return 1;
            }

            case OP_PRINT: {
                printvalue(rkc(i));
                break;
            }
        }
    }

    return 0;
}


// Temp?
BT_API void bt_call(bt_Context* bt, bt_Function* fn)
{
    // return;
    bt_Thread* t = ctx_getthread(bt);
    Call* c = t->call;
    bt_Closure* cl = malloc(sizeof(bt_Closure));
    cl->function = fn;
    c->closure = cl;
    c->ip = fn->program;
    thread_execute(bt, t);
    free(cl);
}