#ifndef _LEX_H_
#define _LEX_H_

#include "bullet_train.h"

enum {
    TK_EOF,
    TK_ID,
    TK_NUMBER,
    TK_NIL, // nil
    TK_TRUE, TK_FALSE, // true, false
    TK_FUNC, TK_TASK, // func, task
    TK_IF, TK_ELIF, TK_ELSE, // if, elif, else
    TK_WHILE, // while
    TK_EQ, TK_NE, TK_LE, TK_ME, // ==, !=, <=, >=
    TK_AND, TK_OR, // &&, ||
    TK_RET, // ret
    TK_PRINT // print
};

typedef struct Lexer Lexer;

Lexer* lex_new(const char* src);
void lex_free(Lexer* lx);

int lex_next(Lexer* lx);
int lex_peek(Lexer* lex);

BT_NUMBER lex_getnumber(Lexer* lx);
const char* lex_gettext(Lexer* lx);

#endif