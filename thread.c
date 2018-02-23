#include <stdlib.h>
#include <stdio.h>

#include "thread.h"
#include "function.h"
#include "context.h"

/*
** Function call information.
** Basically just acts as a stack frame.
** Stored in a linked list so they can be reused, avoiding dynamic allocations.
*/

#if 0

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
    t->sp = t->stack = malloc(sizeof(bt_Value) * 32);
    t->stack_size = 32;
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
static int tobool(bt_Value* vl)
{
    switch (vl->type)
    {
        case VT_BOOL:   return vl->boolean;
        case VT_NUMBER: return vl->number != 0;
        default:        return 0;
    }
}

#define boolval(b) ((bt_Value) { .boolean = (b), .type = VT_BOOL })

/*
** ============================================================
** The interpreter, Bullet Train's heart and soul
** ============================================================
*/

/*
** Main loop of the interpreter
*/
int thread_execute(bt_Context* bt, bt_Thread* t)
{
    Call* c;
    bt_Function* fn;
    // These two are stored locally to slightly speed up access
    bt_Value* sp;
    Instruction* ip;

Refresh:
    c = t->call;
    fn = c->closure->function;
    sp = t->sp;
    ip = c->ip;

    for (;;)
    {
        Instruction i = *ip++;
        switch (i & 0xFF)
        {
            default:
                break;
        }
    }

    return 0;
}

#endif

// Temp?
BT_API void bt_call(bt_Context* bt, bt_Function* fn)
{
    /* bt_Thread* t = ctx_getthread(bt);
    Call* c = t->call;
    bt_Closure* cl = malloc(sizeof(bt_Closure));
    cl->function = fn;
    c->closure = cl;
    c->ip = fn->program;
    t->sp = c->base + fn->locals;
    thread_execute(bt, t);
    free(cl); */
}