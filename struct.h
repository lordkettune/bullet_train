#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "bullet_train.h"
#include "value.h"

typedef struct Metatable Metatable;
typedef struct Key Key;

struct bt_Struct {
    Metatable* meta;
    bt_Value* data;
    int size;
};

Metatable* newrootmeta();
void destroystruct(void* st);

void setstruct(bt_Struct* s, Key* k, bt_Value* vl);
bt_Value getstruct(bt_Struct* s, Key* k);

#endif