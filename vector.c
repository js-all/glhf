#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "vector.h"

//! NOTE: lots of pointer aritmetics in there as c is not aware of the size of our elements
//! data is written as void* when the size of our elements is stored in vec->data_size

void vector_init(struct Vector *vec, int initSize, size_t dataSize) {
    vec->size = 0;
    vec->allocated = initSize;
    vec->data = calloc(initSize, dataSize);
    vec->data_size = dataSize;
}

void vector_push(struct Vector *vec, void* data) {
    // if used size is larger than allocated
    if(vec->size >= vec->allocated) {
        // reallocated double the size
        vec->data = realloc(vec->data, (vec->allocated*=2) * vec->data_size);
    } 
    // copy the data to push, to manually allocated memory to avoid dangeling pointers
    memcpy(vec->data + (vec->size++) * vec->data_size, data, vec->data_size);
}

void vector_pop(struct Vector *vec, void* data) {
    // if data points somewhere, copy last element's data there
    // and decrements vec->size as well, which is all that is
    // needed to pop really as i will not reallocate to smaller
    // sizes
    vec->size--;
    if(data != NULL)
        memcpy(data, vec->data + vec->size * vec->data_size, vec->data_size);
}

void vector_shift(struct Vector *vec, void* data) {
    // copy first element to data if it points somewhere then just
    // shift every other elements once to the left (using memmove)
    if(data != NULL) 
        memcpy(data, vec->data, vec->data_size);
    if(--vec->size > 0)
        memmove(vec->data, vec->data + vec->data_size, vec->size * vec->data_size);
}

void vector_print_as_int(struct Vector *vec) {
    printf("[");
    for(int i = 0; i < vec->size; i++) {
        if(i == 0) { // then no ned for the coma
            printf("%i", vector_get(vec->data, i, int));
        } else{
            printf(", %i", vector_get(vec->data, i, int));
        }
    }
    printf("]");
}

void vector_free(struct Vector *vec) {
    free(vec->data);
}
