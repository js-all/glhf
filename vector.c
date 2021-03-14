#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "vector.h"

void vector_init(struct Vector *vec, int initSize, size_t dataSize) {
    vec->size = 0;
    vec->allocated = initSize;
    vec->data = calloc(initSize, sizeof(void*));
    vec->data_size = dataSize;
}

void vector_push(struct Vector *vec, void* data) {
    if(vec->size >= vec->allocated) {
        vec->data = realloc(vec->data, (vec->allocated*=2) * sizeof(void*));
    } 
    void* new = malloc(vec->data_size);
    memcpy(new, data, vec->data_size);
    vec->data[vec->size++] = new;
}

void vector_pop(struct Vector *vec, void* data) {
    if(data != NULL)
        memcpy(data, vec->data[--vec->size], vec->data_size);
    else
        vec->size--;
    free(vec->data[vec->size]);
}

void vector_shift(struct Vector *vec, void* data) {
    if(data != NULL) 
        memcpy(data, vec->data[0], vec->data_size);
    free(vec->data[0]);
    vec->size--;
    for(int i = 0; i < vec->size; i++) {
        vec->data[i] = vec->data[i+1];
    }
}

void vector_print_as_int(struct Vector *vec) {
    printf("[");
    for(int i = 0; i < vec->size; i++) {
        if(i == 0) {
            printf("%i", *(int*) vec->data[i]);
        } else{
            printf(", %i", *(int*) vec->data[i]);
        }
    }
    printf("]");
}

void vector_free(struct Vector *vec) {
    for(int i = 0; i < vec->size; i++) {
        free(vec->data[i]);
    }
    free(vec->data);
}
