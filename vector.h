#ifndef size_t
#include <stdlib.h>
#endif

#define vector_get(vectordata, index, type) (*(type*)vectordata[index])
#define vector_push_array(vector, array, size) \
for(int _i = 0; _i < size; _i++) { \
    vector_push(&vector, &array[_i]); \
}
#define vector_to_array(vector, array, type) \
for(int _i = 0; _i < vector.size; _i++) { \
    array[_i] = vector_get(vector.data, _i, type); \
}

struct Vector {
    void** data;
    int size;
    int allocated;
    size_t data_size;
};

typedef struct Vector Vector;

void vector_init(struct Vector *vec, int initSize, size_t dataSize);
void vector_push(struct Vector *vec, void* data);
void vector_pop(struct Vector *vec, void* data);
void vector_shift(struct Vector *vec, void* data);
void vector_free(struct Vector *vec);
void vector_print_as_int(struct Vector *vec);