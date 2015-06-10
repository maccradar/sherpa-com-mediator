#ifndef PROXY_DEFS
#define PROXY_DEFS "Definitions"

// Dependencies
#include "rfsmbinding.h"
#include <jansson.h>
#include <time.h>
#include <zyre.h>

#define TIMEOUT 1000

// Global lua state
lua_State *L;

typedef struct {
    char* metamodel_id;
    char* model_id;
    char* event_id;
    char* timestamp;
} event_t;

static char* generate_json(event_t* e) {
  json_t *root = json_array();
  json_t *event = json_object();
  json_t *event_data = json_object();
  json_object_set_new( event, "timestamp", json_string(e->timestamp));
  json_object_set_new( event, "event", event_data);
  json_object_set_new( event_data, "metamodel_id", json_string(e->metamodel_id));
  json_object_set_new( event_data, "model_id", json_string(e->model_id));
  json_object_set_new( event_data, "event_id", json_string(e->event_id));
  json_array_append( root, event );
  
  char* ret_strings = json_dumps( root, 0 );
  json_decref(root);
  return ret_strings;
}

typedef struct {
    char *name;
    char *group;
    zyre_t *node; // pointer to Zyre node
    zsock_t *pipe; // main loop socket
    zpoller_t *com; // poller to check communication channels
    zlist_t *input_events; // list of input events
    zlist_t *output_events; // list of output events
    char *input; // latest received input
    char *userinput; // latest user input
    bool deleted;
    bool configured_resources;
} resource_t;

#endif
