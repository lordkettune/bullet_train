#ifndef _THREAD_H_
#define _THREAD_H_

#include "bullet_train.h"
#include "value.h"

typedef struct Call Call;

struct bt_Thread {
    bt_Thread* next;
    BT_TIMER timer;
    bt_Value* stack;
    int stacksize;
    Call* call;
};

bt_Thread* thread_new();
int thread_execute(bt_Context* bt, bt_Thread* t);

#endif