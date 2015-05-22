#ifndef SHERPA_DEFS
#define SHERPA_DEFS "SHERPA Definitions"

// SHERPA Protocol constants for signalling
#define SHERPA_HEARTBEAT "HEARTBEAT"      //  Signals device heartbeat
#define SHERPA_PROXY "PROXY"
// Resource definitions
#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  1000    //  msecs
#define INTERVAL_INIT       1000    //  Initial reconnect
#define INTERVAL_MAX       32000    //  After exponential backoff

// Dependencies
#include "czmq.h"
#include "rfsmbinding.h"
#include <jansson.h>
#include <time.h>

// Global lua state
lua_State *L;

typedef struct {
    char* metamodel_id;
    char* model_id;
    char* event_id;
    char* timestamp;
} event_t;

typedef struct {
    zframe_t *identity;         //  Identity of resource
    char *id_string;            //  Printable identity
    int64_t expiry;             //  Expires at this time
} backend_resource_t;

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

static char* generate_json_ready(char* name) {

}

static char* generate_json_heartbeat(char* name) {
  char *ret_strings = NULL;
  time_t rawtime;
  struct tm *info;
  time( &rawtime );
  info = localtime( &rawtime );
  char timestamp[80];
  sprintf(timestamp,"%d%d%d%d%d%d",info->tm_year+1900, info->tm_mon+1, info->tm_mday,info->tm_hour, info->tm_min,info->tm_sec);
  json_t *root = json_array();
  json_t *event = json_object();
  json_t *event_data = json_object();
  json_object_set_new( event, "timestamp", json_string(timestamp));
  json_object_set_new( event, "event", event_data);
  json_object_set_new( event_data, "metamodel_id", json_string(SHERPA_PROXY));
  json_object_set_new( event_data, "model_id", json_string(name));
  json_object_set_new( event_data, "event_id", json_string(SHERPA_HEARTBEAT));
  json_array_append( root, event );
  
  ret_strings = json_dumps( root, 0 );
  json_decref( root );
  return ret_strings;
}

//  Construct new resource, i.e. new local object representing a resource at the backend
static backend_resource_t *
s_backend_resource_new (zframe_t *identity, char* name)
{
    backend_resource_t *self = (backend_resource_t *) zmalloc (sizeof (backend_resource_t));
    self->identity = identity;
    self->id_string = name; //zframe_strhex (identity);
    self->expiry = zclock_time ()
                 + HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS;
    return self;
}

//  Destroy specified backend_resource object, including identity frame.
static void
s_backend_resource_destroy (backend_resource_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        backend_resource_t *self = *self_p;
        zframe_destroy (&self->identity);
        free (self->id_string);
        free (self);
        *self_p = NULL;
    }
}

//  The ready method puts a backend_resource to the end of the ready list:

static void
s_backend_resource_ready (backend_resource_t *self, zlist_t *backend_resources)
{
    backend_resource_t *backend_resource = (backend_resource_t *) zlist_first (backend_resources);
    while (backend_resource) {
        if (streq (self->id_string, backend_resource->id_string)) {
            zlist_remove (backend_resources, backend_resource);
            s_backend_resource_destroy (&backend_resource);
            break;
        }
        backend_resource = (backend_resource_t *) zlist_next (backend_resources);
    }
    zlist_append (backend_resources, self);
}

//  The next method returns the next available backend_resource identity:
//  TODO: Not all backend_resources have the same capabilities so we should not just pick any backend_resource...

static zframe_t *
s_backend_resources_next (zlist_t *backend_resources)
{
    backend_resource_t *backend_resource = zlist_pop (backend_resources);
    assert (backend_resource);
    zframe_t *frame = backend_resource->identity;
    backend_resource->identity = NULL;
    s_backend_resource_destroy (&backend_resource);
    return frame;
}

//  The purge method looks for and kills expired backend_resources. We hold backend_resources
//  from oldest to most recent, so we stop at the first alive backend_resource:
//  TODO: if backend_resources expire, it should be checked if this effects the working of the line!

static void
s_backend_resources_purge (zlist_t *backend_resources)
{
    backend_resource_t *backend_resource = (backend_resource_t *) zlist_first (backend_resources);
    while (backend_resource) {
        if (zclock_time () < backend_resource->expiry)
            break;              //  backend_resource is alive, we're done here
	printf("Removing expired backend_resource %s\n", backend_resource->id_string);
        zlist_remove (backend_resources, backend_resource);
        s_backend_resource_destroy (&backend_resource);
        backend_resource = (backend_resource_t *) zlist_first (backend_resources);
    }
}

typedef struct {
    char *name;
    char *frontend_str; // frontend endpoint. e.g. tcp://localhost:9001
    char *backend_str; // backend endpoint. e.g. tcp://*:9002
    zsock_t *frontend; // frontend socket to remote peers
    bool frontend_connected; // keep track of frontend
    zsock_t *backend; // backend socket to local workers
    zsock_t *pipe; // main loop socket
    zpoller_t *com; // poller to check communication channels
    zlist_t *input_events; // list of input events
    zlist_t *output_events; // list of output events
    char *input; // latest received input
    char *userinput; // latest user input
    size_t liveness; // liveness defines how many heartbeat failures are tolerable
    size_t interval; // interval defines at what interval heartbeats are sent
    uint64_t heartbeat_at; // heartbeat_at defines when to send next heartbeat
    zlist_t *backend_resources;
    bool configured_resources;
    bool deleted;
} resource_t;

#endif
