#include "stack.h"
#include "vector.h"
#include <stdio.h>
#include <string.h>

void stack_init(struct Stack *stk, size_t dataSize) {
    vector_init(&stk->vec, 5, dataSize);
    stk->top = stk->vec.data - dataSize;
}

void stack_push(struct Stack *stk, void* data) {
    vector_push(&stk->vec, data);
    stk->top += stk->vec.data_size;
}

void stack_pop(struct Stack *stk, void* data) {
    vector_pop(&stk->vec, data);
    stk->top -= stk->vec.data_size;
}

void stack_move_to_top(struct Stack *stk, int index) {
    void* data = vector_get_pointer_to(stk->vec, index);
    vector_push(&stk->vec, data);
    vector_splice(&stk->vec, index, 1, NULL);
}

bool stack_is_empty(struct Stack stk) {
    return stk.vec.size == 0;
}

void stack_peek(struct Stack *stk, void* data) {
    if(data == NULL || stk->vec.size < 1) return;
    memcpy(data, stk->top, stk->vec.data_size);
}

void stack_print_as_int(struct Stack *stk) {
    vector_print_as_int(stk->vec);
}

void stack_free(struct Stack *stk) {
    free(stk->vec.data);
}
