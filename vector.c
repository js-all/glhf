#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "vector.h"

//TODO rework this to remove the double malloc.

void vector_init(struct Vector *vec, int initSize, size_t dataSize) {
    vec->size = 0;
    vec->allocated = initSize;
    vec->data = calloc(initSize, sizeof(void*));
    vec->data_size = dataSize;
}

void vector_push(struct Vector *vec, void* data) {
    // if used size is larger than allocated
    if(vec->size >= vec->allocated) {
        // reallocated double the size
        vec->data = realloc(vec->data, (vec->allocated*=2) * sizeof(void*));
    } 
    // allocate memory for each element
    void* new = malloc(vec->data_size);
    // copy data there, less effiecient but protect from any dangeling pointers
    memcpy(new, data, vec->data_size);
    vec->data[vec->size++] = new;
}

void vector_pop(struct Vector *vec, void* data) {
    // if data points somewhere, copy last element's data there
    // and decrements vec->size as well, which is all that is
    // needed to pop (with freeing the popped element) really
    // as i will not reallocate to smaller sizes
    vec->size--;
    if(data != NULL)
        memcpy(data, vec->data[vec->size], vec->data_size);
    // here, we are freeing the allocated memory for the element
    // specifically, not the array in itself
    free(vec->data[vec->size]);
}

void vector_shift(struct Vector *vec, void* data) {
    // lazy implementation, copy first element to data if it points somewhere
    // free the first element and just shift every other once to the left
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
        if(i == 0) { // then no ned for the coma
            printf("%i", *(int*) vec->data[i]);
        } else{
            printf(", %i", *(int*) vec->data[i]);
        }
    }
    printf("]");
}

void vector_free(struct Vector *vec) {
    // first free every allocated memory for each elements
    for(int i = 0; i < vec->size; i++) {
        free(vec->data[i]);
    }
    // then free the allocated memory for the array itself
    free(vec->data);
}
