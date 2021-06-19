#ifndef _STACK_H
#define _STACK_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "vector.h"
struct Stack  {
    Vector vec;
    void* top;
};

void stack_init(struct Stack *stk, size_t dataSize);
void stack_push(struct Stack *stk, void* data);
void stack_pop(struct Stack *stk, void* data);
bool stack_is_empty(struct Stack stk);
void stack_peek(struct Stack *stk, void* data);
void stack_print_as_int(struct Stack *stk);
void stack_free(struct Stack *stk);
void stack_move_to_top(struct Stack *stk, int index);
#endif