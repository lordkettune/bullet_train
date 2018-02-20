#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bullet_train.h"
#include "lex.h"
#include "function.h"

typedef struct Local Local;

struct Local {
    Local* prev;
    int scope;
    int idx;
    char name[];
};

typedef struct {
    bt_Context* ctx;
    bt_Function* fn;
    Lexer* lx;
    // Hideous vector counters. Not much I can do since this ain't C++
    int ps, pr; // Program size, program reserved
    int ds, dr; // Data size, data reserved
    Local* locals;
} Parser;

static void initparser(Parser* p, bt_Context* ctx, Lexer* lx)
{
    bt_Function* fn = malloc(sizeof(bt_Function));
    fn->program = malloc(sizeof(Instruction) * 4);
    fn->data = malloc(sizeof(FData) * 4);
    fn->next = NULL;
    fn->params = 0;
    fn->locals = 0;
    fn->type = FT_FUNC;
    p->fn = fn;
    p->lx = lx;
    p->ctx = ctx;
    p->ps = 0; p->pr = 4;
    p->ds = 0; p->dr = 4;
    p->locals = NULL;
}

/* Adds an instruction to the result */
static void addop(Parser* p, Instruction i)
{
    bt_Function* fn = p->fn;
    fn->program[p->ps++] = i;
    if (p->ps == p->pr) {
        p->pr *= 2;
        fn->program = realloc(fn->program, sizeof(Instruction) * p->pr);
    }
}

/* Returns the index of an empty instruction to be set later */
static inline int reserve(Parser* p)
{
    addop(p, 0);
    return p->ps - 1;
}

// Shortcuts for adding instructions with arguments
#define addopa(p, i, a)     addop(p, (i) | ((a) << 16))
#define addopab(p, i, a, b) addop(p, (i) | ((b) << 8) | ((a) << 16))

/*
** Adds an FData to the result, returning the index of the item
*/
static int adddata(Parser* p, FData d)
{
    bt_Function* fn = p->fn;
    fn->data[p->ds++] = d;
    if (p->ds == p->dr) {
        p->dr *= 2;
        fn->data = realloc(fn->data, sizeof(FData) * p->dr);
    }
    return p->ds - 1;
}

#define addvalue(p, v) adddata(p, (FData) { .value = (v) })

/*
** Searches for a local variable in the parser's list.
** Returns NULL if it doesn't exist.
*/
static Local* findlocal(Parser* p, const char* name)
{
    Local* l = p->locals;
    while (l != NULL) {
        if (strcmp(l->name, name) == 0) {
            return l;
        }
        l = l->prev;
    }
    return NULL;
}

/* Creates a new local and adds it to the parser's list */
static Local* newlocal(Parser* p, const char* name)
{
    Local* l = malloc(sizeof(Local) + strlen(name) + 1);
    strcpy(l->name, name);
    Local* last = p->locals;
    l->idx = last ? last->idx + 1 : 0;
    l->prev = last;
    l->scope = 0;
    p->locals = l;
    ++p->fn->locals;
    return l;
}

/*
** Errors if the next token isn't [tk]
*/
static void expect(Parser* p, int tk)
{
    if (lex_next(p->lx) != tk) {

    }
}

/*
** If the next token is [tk], advances the lexer and returns true
*/
static int accept(Parser* p, int tk)
{
    if (lex_peek(p->lx) == tk) {
        lex_next(p->lx);
        return 1;
    }
    return 0;
}

/*
** ============================================================
** Recursive descent parser stuff
** ============================================================
*/

static inline void expression(Parser* p);
static void statement(Parser* p);

/*
** Adds a value to the function data, as well as a push instruction
*/
static void literal(Parser* p, bt_Value v)
{
    int index = adddata(p, (FData) { .value = v });
    addopa(p, OP_PUSH, index);
}

/*
** Smallest unit of parsing.
** Literals, function calls, things in parentheses
*/
static void atom(Parser* p)
{
    switch (lex_next(p->lx))
    {
        case TK_NUMBER:
            literal(p, (bt_Value) { .number = lex_getnumber(p->lx), .type = VT_NUMBER });
            break;
        case TK_TRUE:
            addopa(p, OP_PUSHBOOL, 1);
            break;
        case TK_FALSE:
            addopa(p, OP_PUSHBOOL, 0);
            break;
        case TK_NIL:
            addop(p, OP_PUSHNIL);
            break;
        case TK_ID: {
            const char* name = lex_gettext(p->lx);
            Local* l = findlocal(p, name);
            addopa(p, OP_LOAD, l->idx);
            break;
        }
        case '!':
            expression(p);
            addop(p, OP_NOT);
            break;
        case '-':
            expression(p);
            addop(p, OP_NEG);
            break;
        case '(':
            expression(p);
            expect(p, ')');
            break;
    }
}

#define optrue  (1 << 16)
#define opfalse (0 << 16)

/*
** Mathematical/logical expression.
** Uses the precedence climbing algorithm.
*/
static void exprclimb(Parser* p, int min)
{
    atom(p);
    for (;;) {
        int prec;
        Instruction inst;
        switch (lex_peek(p->lx))
        {
            case '*':    prec = 6; inst = OP_MUL;   break;
            case '/':    prec = 6; inst = OP_DIV;   break;
            case '+':    prec = 5; inst = OP_ADD;   break;
            case '-':    prec = 5; inst = OP_SUB;   break;
            case '>':    prec = 4; inst = OP_LEQUAL | opfalse; break;
            case '<':    prec = 4; inst = OP_LESS   | optrue;  break;
            case TK_ME:  prec = 4; inst = OP_LESS   | opfalse; break;
            case TK_LE:  prec = 4; inst = OP_LEQUAL | optrue;  break;
            case TK_NE:  prec = 3; inst = OP_EQUAL  | opfalse; break;
            case TK_EQ:  prec = 3; inst = OP_EQUAL  | optrue;  break;
            case TK_AND: prec = 2; inst = OP_AND;   break;
            case TK_OR:  prec = 1; inst = OP_OR;    break;
            default: return;
        }
        if (prec >= min) {
            lex_next(p->lx);
            exprclimb(p, prec + 1);
            addop(p, inst);
        } else
            return;
    }
}

/* Just a shortcut */
static inline void expression(Parser* p) { exprclimb(p, 0); }

/*
** Variable set/declaration or function call
*/
static void varstmt(Parser* p)
{
    const char* name = lex_gettext(p->lx);
    Local* l = findlocal(p, name);
    if (l == NULL) {
        l = newlocal(p, name);
    }
    expect(p, '=');
    expression(p);
    addopa(p, OP_STORE, l->idx);
}

/*
** A bunch of statements within a pair of brackets
*/
static void block(Parser* p)
{
    expect(p, '{');
    while (!accept(p, '}')) {
        statement(p);
    }
}

/* Sets the instruction at [i] as a jump to the parser's current instruction */
#define setjump(p, i, op) p->fn->program[i] = op | (p->ps << 16)

/*
** If-elif-else block
*/
static void conditionblock(Parser* p)
{
    expression(p);
    int jump = reserve(p); // Position of the JUMPIF instruction
    block(p);
    int tk = lex_peek(p->lx);
    if (tk == TK_ELIF || tk == TK_ELSE) {
        lex_next(p->lx);
        int end = reserve(p); // Position of the JUMP to the end of the block
        setjump(p, jump, OP_JUMPIF);
        if (tk == TK_ELIF) {
            conditionblock(p);
        } else {
            block(p);
        }
        setjump(p, end, OP_JUMP);
    } else {
        setjump(p, jump, OP_JUMPIF);
    }
}

static void whileblock(Parser* p)
{
    int position = p->ps; // Start of the while loop
    expression(p);
    int jump = reserve(p); // Position of the JUMP back to the start
    block(p);
    addopa(p, OP_JUMP, position);
    setjump(p, jump, OP_JUMPIF);
}

static void statement(Parser* p)
{
    switch (lex_next(p->lx))
    {
        case TK_PRINT:
            expression(p);
            addop(p, OP_PRINT);
            break;
        case TK_ID:
            varstmt(p);
            break;
        case TK_IF:
            conditionblock(p);
            break;
        case TK_WHILE:
            whileblock(p);
            break;
    }
}

/*
** ============================================================
** API functions
** ============================================================
*/

/* Compiles a string to a bt_Function */
BT_API bt_Function* bt_compile(bt_Context* bt, const char* src)
{
    Parser p;
    Lexer* lx = lex_new(src);
    initparser(&p, bt, lx);
    while (lex_peek(lx) != TK_EOF) {
        statement(&p);
    }
    addop(&p, OP_RETURN);
    lex_free(lx);

    bt_Function* fn = p.fn;
/*
    for (int i = 0; i != p.ps; ++i) {
        int op = fn->program[i];
        printf("%i: %i  %i  %i\n", i, op & 0xff, (op >> 8) & 0xff, op >> 16);
    }
*/
    return fn;
}

/* Loads a file and compiles it */
BT_API bt_Function* bt_fcompile(bt_Context* bt, const char* path)
{
    int size;
    FILE* file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    char buffer[size + 1];
    fread(buffer, 1, size, file);
    fclose(file);
    buffer[size] = 0;
    return bt_compile(bt, buffer);
}