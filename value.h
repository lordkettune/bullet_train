#ifndef _VALUE_H_
#define _VALUE_H_

#include "bullet_train.h"

enum {
    VT_NIL,
    VT_NUMBER,
    VT_BOOL,
    VT_CLOSURE,
    VT_STRUCT
};

struct bt_Value {
    union {
        BT_NUMBER number;
        int boolean;
        bt_Closure* closure;
        bt_Struct* struc;
    };
    int type;
};

struct bt_Closure {
    bt_Function* function;
    bt_Value* upvalues[];
};

#endif