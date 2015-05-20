#ifndef PNP_DEFS
#define PNP_DEFS "Pick-n-Pack Definitions"

//  Pick-n-Pack Protocol constants for signalling
#define PNP_READY "READY"    //  Signals device is ready
#define PNP_HEARTBEAT "HEARTBEAT"      //  Signals device heartbeat
#define PNP_PPP "PPP"

// IDs
#define PNP_LINE_ID "010"
#define PNP_THERMOFORMER_ID "011"
#define PNP_ROBOT_CELL_ID "012"
#define PNP_QAS_ID "013"
#define PNP_CEILING_ID "014"
#define PNP_PRINTING_ID "015"

// Names
#define PNP_LINE "Line"
#define PNP_THERMOFORMER "Thermoformer"
#define PNP_ROBOT_CELL "Robot Cell"
#define PNP_QAS "QAS"
#define PNP_CEILING "Ceiling"
#define PNP_PRINTING "Printing"

// Status
#define PNP_CREATING "100"
#define PNP_INITIALISING "101"
#define PNP_CONFIGURING "102"
#define PNP_RUNNING "103"
#define PNP_PAUSING "104"
#define PNP_FINALISING "105"
#define PNP_DELETING "106"

// Signals/commands
#define PNP_RUN "110"
#define PNP_PAUSE "111"
#define PNP_CONFIGURE "112"
#define PNP_STOP "113"
#define PNP_REBOOT "114"
#define PNP_GREEN "115"
#define PNP_ORANGE "116"
#define PNP_RED "117"

// Resource definitions
#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  1000    //  msecs
#define INTERVAL_INIT       1000    //  Initial reconnect
#define INTERVAL_MAX       32000    //  After exponential backoff

#define STACK_MAX 5 // maximum size of transition stack, e.g. running->configuring->initialising->finalising->pausing when configuring cannot proceed without reinit
#define PAYLOAD_MAX 10 // maximum number of payload items in a single transition. E.g. change 10 configuration parameters.

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
  json_object_set_new( event_data, "metamodel_id", json_string(PNP_PPP));
  json_object_set_new( event_data, "model_id", json_string(name));
  json_object_set_new( event_data, "event_id", json_string(PNP_HEARTBEAT));
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
    char *frontend_str; // string representation of frontend socket. e.g. tcp://localhost:9001
    char *backend_str; // string representation of backed socket. e.g. tcp://*:9002
    zsock_t *frontend; // socket to frontend process, e.g. backend_resource
    bool frontend_connected; // keep track of frontend
    zsock_t *backend; // socket to potential backend processes, e.g. subdevices
    zsock_t *pipe; // socket to main loop
    zpoller_t *com; // poller to check communication channels
    zlist_t *input_events; // list of input events
    zlist_t *output_events; // list of output events
    char *input; // latest received input
    char *userinput; // latest user input
    size_t liveness; // liveness defines how many heartbeat failures are tolerable
    size_t interval; // interval defines at what interval heartbeats are sent
    uint64_t heartbeat_at; // heartbeat_at defines when to send next heartbeat
    zlist_t *backend_resources;
    zlist_t *required_resources;
    bool configured_resources;
    bool deleted;
} resource_t;

#endif
