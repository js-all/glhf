#ifndef Vector
#include "vector.h"
#endif

struct EventSubscriber {
    char* event_name;
    void (*event_callback)(void* args);
    int id;
};

struct EventBroadcaster {
    int _eventIdSeed;
    Vector subscibed_events;
};

void events_init(struct EventBroadcaster *ev);
void events_subscribe(struct EventBroadcaster *ev, char* eventName, void (*eventCallback)(void* arg), int* id);
void events_unsubscribe(struct EventBroadcaster *ev, int id);
void events_broadcast(struct EventBroadcaster *ev, char* eventName, void* arg);