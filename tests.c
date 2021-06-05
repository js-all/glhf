// verry dirty code to test the features of thinhgs like vectors
// and make sure they work as intended before using them in the
// main codebase, to avoid having to debug two messes at once.

#include "vector.h"
#include "events.h"
#include "maps.h"
#include <stdio.h>
#include <stdlib.h>

void eventsCallback1(void* arg) {
    int a = *(int*)arg;
    printf("event callback 1 called, arg: %i\n", a);
}

void eventsCallback2(void* arg) {
    int a = *(int*)arg;
    printf("event callback 2 called, arg: %i\n", a);
}

int main() {
    printf("\ntesting vector: basic int\n\n");
    printf("1: initializing vector with initial allocation 5\n");
    struct Vector v;
    vector_init(&v, 5, sizeof(int));
    printf("vector allocated: %i\n", v.allocated);
    printf("vector state: ");
    vector_print_as_int(v);
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
    vector_print_as_int(v);
    printf("\n\n");
    printf("3: filling vector with more value than initial size\nbefore: size: %i, allocated: %i\npushing values...\n", v.size, v.allocated);
    for(int va = 1; va < 6; va++) {
        vector_push(&v, &va);
    }
    printf("vector size: %i, vector allocated: %i\n", v.size, v.allocated);
    printf("vector state: ");
    vector_print_as_int(v);
    printf("\n\n");
    printf("4: testing shifting\nshifting array...\n");
    int shift;
    vector_shift(&v, &shift);
    printf("new vector size: %i, shifted %i\n", v.size, shift);
    printf("vector state: ");
    vector_print_as_int(v);
    printf("\n\n");
    printf("5: testing popping\npopping array...\n");
    int pop;
    vector_pop(&v, &pop);
    printf("new vector size: %i, popped %i\n", v.size, pop);
    printf("vector state: ");
    vector_print_as_int(v);
    printf("\n\n");
    printf("6: testing insert\ninserting 0\n");
    int data = 0;
    vector_insert(&v, &data);
    printf("new vector size: %i\nvector state: ", v.size);
    vector_print_as_int(v);
    printf("\n\n");
    printf("7: testing insert_before\ninserting 40 before index 3\n");
    data = 40;
    vector_insert_before(&v, &data, 3);
    printf("new vector size: %i\nvector state: ", v.size);
    vector_print_as_int(v);
    printf("\n\n");
    printf("8: testing splice\nsplicing from index 3 for 2 element\n");
    vector_splice(&v, 3, 2);
    printf("new vector size: %i\nvector state: ", v.size);
    vector_print_as_int(v);
    printf("\n\n");
    printf("9: testing set\nsetting index 3 to 3\n");
    data = 3;
    vector_set(&v, &data, 3);
    printf("vector state: ");
    vector_print_as_int(v);
    printf("\n\n");
    printf("freeing vector\n");
    vector_free(v);
    printf("\ntesting events\ninitializing EventBroadcaster\n\n");
    struct EventBroadcaster ev;
    events_init(&ev);
    printf("1: testing events_subscribe\nsubscribing two events, one to \"event1\" the other to \"event2\"\n");
    events_subscribe(&ev, "event1", eventsCallback1, NULL);
    int e2id;
    events_subscribe(&ev, "event2", eventsCallback2, &e2id);
    printf("EventBroadcaster's subscribers size: %i\n\n", ev.subscribers.size);
    printf("2: testing events_broadcast\nbroadcasting \"event1\" (arg = 12) and \"event2\" (arg = 6) \n");
    int evl = 12;
    events_broadcast(&ev, "event1", &evl);
    evl = 6;
    events_broadcast(&ev, "event2", &evl);
    printf("\n\n3: testing events_unsubscribe\nunsubscribing 2nd subscribed event\n");
    events_unsubscribe(&ev, e2id);
    printf("new EventBroadcaster's subscribers size: %i\n\n", ev.subscribers.size);
    printf("4: retesting events_broadcast after unsubscribing\nbroadcasting \"event1\" (arg = 12) and \"event2\" (arg = 6) \n");
    evl = 12;
    events_broadcast(&ev, "event1", &evl);
    evl = 6;
    events_broadcast(&ev, "event2", &evl);
    printf("\nfreeing events...\n");
    events_free(&ev);
    printf("\nTesting maps\ninitializing map\n\n");
    Map map;
    map_init(&map, sizeof(int));
    printf("1: testing map_set\nsetting key \"key1\" to 25\n");
    int mv = 25;
    map_set(&map, "key1", &mv);
    printf("map: ");
    map_print(map, int, "%i");
    printf("\n\n2: testing map_get\ngetting \"key1\"\n");
    int mpv;
    map_get(&map, "key1", &mpv);
    printf("found value: %i\ntrying to get unset key \"key2\"\n", mpv);
    map_get(&map, "key2", &mpv);
    printf("found value: %i\n", mpv);
    printf("map: ");
    map_print(map, int, "%i");
    printf("\n\n");
    printf("3 testing map_delete\ntrying to delete unset key \"key2\"\n");
    map_delete(&map, "key2");
    printf("deleting key \"key1\"\n");
    map_delete(&map, "key1");
    printf("map: ");
    map_print(map, int, "%i");
    printf("\n\n");
    printf("freeing map...\n");
    map_free(&map);
}
