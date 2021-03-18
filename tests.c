// verry dirty code to test the features of thinhgs like vectors
// and make sure they work as intended before using them in the
// main codebase, to avoid having to debug two messes at once.

#include "vector.h"
#include "events.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("\ntesting vector: basic int\n\n");
    printf("1: initializing vector with initial allocation 5\n");
    struct Vector v;
    vector_init(&v, 5, sizeof(int));
    printf("vector allocated: %i\n", v.allocated);
    printf("vector state: ");
    vector_print_as_int(&v);
    printf("\n\n");
    printf("2: test dangeling pointers\n");
    {
        int val = 15;
        printf("pushing 15 in empty initialized vector in inner scope\n");
        vector_push(&v, &val);
        printf("value in scope: %i\n", vector_get(v.data, 0, int));
    }
    printf("value out of scope: %i\n", vector_get(v.data, 0, int));
    printf("vector state: ");
    vector_print_as_int(&v);
    printf("\n\n");
    printf("3: filling vector with more value than initial size\nbefore: size: %i, allocated: %i\npushing values...\n", v.size, v.allocated);
    for(int va = 1; va < 6; va++) {
        vector_push(&v, &va);
    }
    printf("vector size: %i, vector allocated: %i\n", v.size, v.allocated);
    printf("vector state: ");
    vector_print_as_int(&v);
    printf("\n\n");
    printf("4: testing shifting\nshifting array...\n");
    int shift;
    vector_shift(&v, &shift);
    printf("new vector size: %i, shifted %i\n", v.size, shift);
    printf("vector state: ");
    vector_print_as_int(&v);
    printf("\n\n");
    printf("5: testing popping\npopping array...\n");
    int pop;
    vector_pop(&v, &pop);
    printf("new vector size: %i, popped %i\n", v.size, pop);
    printf("vector state: ");
    vector_print_as_int(&v);
    printf("\n\n");
    printf("6: testing insert\ninserting 0\n");
    int data = 0;
    vector_insert(&v, &data);
    printf("new vector size: %i\nvector state: ", v.size);
    vector_print_as_int(&v);
    printf("\n\n");
    printf("7: testing insert_before\ninserting 40 before index 3\n");
    data = 40;
    vector_insert_before(&v, &data, 3);
    printf("new vector size: %i\nvector state: ", v.size);
    vector_print_as_int(&v);
    printf("\n\n");
    printf("8: testing splice\nsplicing from index 3 for 2 element\n");
    vector_splice(&v, 3, 2);
    printf("new vector size: %i\nvector state: ", v.size);
    vector_print_as_int(&v);
    printf("\n\n");
    printf("freeing vector\n");
    vector_free(&v);
    printf("\ntesting events\n\n");
    struct EventBroadcaster ev;
}