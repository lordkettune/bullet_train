#ifndef _STRUCT_H_
#define _STRUCT_H_

#include "bullet_train.h"
#include "value.h"

typedef struct Metatable Metatable;

struct bt_Struct {
    Metatable* meta;
    bt_Value* data;
    int size;
    int reserved;
};

Metatable* struct_newmeta();
void struct_destroy(void* st);

#endif