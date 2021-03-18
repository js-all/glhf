#include "events.h"

void events_init(struct EventBroadcaster *ev) {
    vector_init(&ev->subscibed_events, 5, sizeof(struct EventSubscriber));
    ev->_eventIdSeed = 0;
}

void events_subscribe(struct EventBroadcaster *ev, char* eventName, void (*eventCallback)(void* arg), int* id) {
    struct EventSubscriber es;
    es.event_name = eventName;
    es.event_callback = eventCallback;
    es.id = ev->_eventIdSeed++;
    vector_push(&ev->subscibed_events, &es);
    if(id != NULL)
        *id = es.id;
}

void events_unsubscribe(struct EventBroadcaster *ev, int id) {
    for(int i = 0; i < ev->subscibed_events.size; i++) {
        if(vector_get(ev->subscibed_events.data, i, struct EventSubscriber).id == id) {
            vector_splice(&ev->subscibed_events, i, 1);
        }
    }
}

void events_broadcast(struct EventBroadcaster *ev, char* eventName, void* arg) {
    for(int i = 0; i < ev->subscibed_events.size; i++) {
        struct EventSubscriber es = vector_get(ev->subscibed_events.data, i, struct EventSubscriber);
        if(es.event_name == eventName) {
            (*es.event_callback)(arg);
        }
    }
}