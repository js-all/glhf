#include "maps.h"
#include <string.h>
#include <stdio.h>

void map_init(Map *map, size_t dataSize) {
    map->dataSize = dataSize;
    vector_init(&map->keyVector, 5, sizeof(char*));
    vector_init(&map->valuesVector, 5, dataSize);
}

int getKeyIndex(Map *map, char* key) {
    for(int i = 0; i < map->keyVector.size; i++) {
        if(strcmp(vector_get(map->keyVector.data, i, char*), key) == 0) {
            return i;
        }
    }
    return -1;
}

bool map_has(Map *map, char* key) {
    return getKeyIndex(map, key) != -1;
}

void map_set(Map *map, char* key, void* value) {
    int i = getKeyIndex(map, key);
    if(i != -1) {
        vector_set(&map->valuesVector, value, i);
    } else {
        vector_push(&map->keyVector, &key);
        vector_push(&map->valuesVector, value);
    }
}

void map_get(Map *map, char* key, void* data) {
    int i = getKeyIndex(map, key);
    if(i != -1) {
        memcpy(data, map->valuesVector.data + i * map->dataSize, map->dataSize);
    } else {
        printf("WARN: trying to get unset value in map\n");
    }
}

void map_delete(Map *map, char* key) {
    int i = getKeyIndex(map, key);
    if(i != -1) {
        vector_splice(&map->keyVector, i, 1, NULL);
        vector_splice(&map->valuesVector, i, 1, NULL);
    } else {
        printf("WARN: trying to delete unset key in map (double delete ?)\n");
    }
}

void map_free(Map *map) {
    vector_free(map->keyVector);
    vector_free(map->valuesVector);
}