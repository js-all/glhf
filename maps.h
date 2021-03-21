#include "vector.h"
#include <ctype.h>
#include <stdbool.h>

#define map_print(map, type, format) \
printf("{\n"); \
for(int __i = 0; __i < map.keyVector.size; __i++) { \
    printf("   \"%s\": ", vector_get(map.keyVector.data, __i, char*)); \
    printf(format, vector_get(map.valuesVector.data, __i, type)); \
    printf("\n"); \
} \
printf("}")

typedef struct {
    Vector keyVector;
    Vector valuesVector;
    size_t dataSize;
} Map;

void map_init(Map *map, size_t dataSize);
void map_set(Map *map, char* key, void* value);
bool map_has(Map *map, char* key);
void map_get(Map *map, char* key, void* data);
void map_delete(Map *map, char* key);
void map_free(Map *map);