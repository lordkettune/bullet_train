#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include "bullet_train.h"

typedef struct Key Key;

/*
** Key used to access struct members.
** bt_Context keeps a registry of these, ensuring there are no duplicates.
** This allows Keys to be compared by pointer rather than by string!
*/
struct Key {
    unsigned long hash;
    Key* next;
    char text[];
};

Key* ctx_getkey(bt_Context* bt, const char* name);

bt_Thread* ctx_getthread(bt_Context* bt);

#endif