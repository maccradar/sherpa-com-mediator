#include <czmq.h>
#include <jansson.h>
#include <time.h>

#define BEACON_VERSION 0x01

typedef struct {
    byte protocol [6];
    byte uuid [ZUUID_LEN];
    byte version;
    uint16_t port;
} beacon_t;

typedef struct {
    char* metamodel_id;
    char* model_id;
    char* event_id;
    char* timestamp;
} event_t;

static char* generate_json_heartbeat(char* name) {
  char *ret_strings = NULL;
  
time_t rawtime;
  time( &rawtime );
  json_t *root = json_array();
  json_t *event = json_object();
  json_t *event_data = json_object();
//  json_object_set_new( event, "timestamp", json_string(rawtime));
  json_object_set_new( event, "event", event_data);
  json_object_set_new( event_data, "metamodel_id", json_string("QOS"));
  json_object_set_new( event_data, "model_id", json_string(name));
  json_object_set_new( event_data, "event_id", json_string("HEARTBEAT"));
  json_array_append( root, event ); 
  ret_strings = json_dumps( root, 0 );
  json_decref( root );
  return ret_strings;
}

static event_t decode_json(char* message) {
    json_t *root;
    json_error_t error;
    root = json_loads(message, 0, &error);
    
    if(!root) {
   	printf("Error parsing JSON string! line %d: %s\n", error.line, error.text);
    	return;
    }
    if(!json_is_array(root)) {
        printf("Error: root is not an array\n");
        json_decref(root);
        return;
    }
        event_t e;
        json_t *data, *timestamp, *event, *metamodel_id, *model_id, *event_id;
        const char *metamodel_id_text, *model_id_text, *event_id_text;

        data = json_array_get(root, 0);
        if(!json_is_object(data))
        {
            printf("Error: event data is not an object\n");
            json_decref(root);
            return;
        }

        timestamp = json_object_get(data, "timestamp");
        if(!json_is_string(timestamp)) {
            printf("Error: timestamp is not a string\n");
            json_decref(root);
	    return;
        }

        event = json_object_get(data, "event");
        if(!json_is_object(event)) {
            printf("Error: event is not an object\n");
            json_decref(root);
            return;
        }

        metamodel_id = json_object_get(event, "metamodel_id");
        if(!json_is_string(metamodel_id)) {
            printf("Error: metamodel_id is not a string\n");
            json_decref(root);
            return;
        }

        model_id = json_object_get(event, "model_id");
        if(!json_is_string(model_id)) {
            printf("Error: model_id is not a string\n");
            json_decref(root);
            return;
        }

        event_id = json_object_get(event, "event_id");
        if(!json_is_string(event_id)) {
            printf("Error: event_id is not a string\n");
            json_decref(root);
            return;
        }
        e.timestamp = strdup(json_string_value(timestamp));
        e.metamodel_id = strdup(json_string_value(metamodel_id));
	e.model_id = strdup(json_string_value(model_id));
        e.event_id = strdup(json_string_value(event_id));
 
        printf("JSON parser: (%s) - %s - %s - %s\n", e.timestamp, e.metamodel_id, e.model_id, e.event_id);
    json_decref(root);
    return e;
}
int main(void) {
char* hb = generate_json_heartbeat("node1");
zsock_t *inbox = zsock_new (ZMQ_ROUTER);
// connect monitor to the inbox
zactor_t *qosmon = zactor_new (zmonitor, inbox);
assert (qosmon);
zstr_sendx (qosmon, "VERBOSE", NULL);
zstr_sendx (qosmon, "LISTEN", "ALL", NULL);
zstr_sendx (qosmon, "START", NULL);
zsock_wait (qosmon);

zactor_t *node = zactor_new (zbeacon, NULL);
assert (node1);
zsock_send (node, "si", "CONFIGURE", 5670);
char* hostname = zstr_recv (node1);
assert (*hostname);
int port = zsock_bind (inbox, "tcp://%s:*", hostname);
printf("Node: listening on %s:%i\n", hostname, port);
free (hostname);

beacon_t beacon;
beacon.protocol [0] = 'S';
beacon.protocol [1] = 'H';
beacon.protocol [2] = 'E';
beacon.protocol [3] = 'R';
beacon.protocol [4] = 'P';
beacon.protocol [5] = 'A';
beacon.version = BEACON_VERSION;
beacon.port = htons (port);
zuuid_t *uuid = zuuid_new();
zuuid_export (uuid, beacon.uuid);

zsock_send (node, "sbi", "PUBLISH", (byte *) &beacon, sizeof(beacon_t), 1000);
printf("Node: publishing sherpa beacon\n");
zsock_send (node, "sb", "SUBSCRIBE", "SHERPA", 6);
printf("Node: filtering on SHERPA beacons\n");

zpoller_t *poller = zpoller_new (node, inbox, NULL);
assert (poller);
int64_t stop_at = zclock_mono () + 5000;
while (zclock_mono () < stop_at) {
    long timeout = (long) (stop_at - zclock_mono ());
    if (timeout < 0)
        timeout = 0;
    void *which = zpoller_wait (poller, timeout * ZMQ_POLL_MSEC);
    if (which) {
	if(which == node) {
        	char *ipaddress = zstr_recv (which);
        	printf("Node: received from ip %s\n", ipaddress);
    		zframe_t *frame = zframe_recv (which);
    		if (ipaddress == NULL) {
        		printf("Node: got interrupted\n");                 //  Interrupted
			break;
		}
    		//  Ignore anything that isn't a valid beacon
    		beacon_t beacon = { { 0 } };
    		if (zframe_size (frame) == sizeof (beacon_t))
        		memcpy (&beacon, zframe_data (frame), zframe_size (frame));
    		zframe_destroy (&frame);
    		if (beacon.version != BEACON_VERSION) {
        		printf("Node: Invalid beacon version\n");                 //  Garbage beacon, ignore it
			break;
		}
    		zuuid_t *uuid = zuuid_new ();
    		zuuid_set (uuid, beacon.uuid);
    		if (beacon.port) {
        		char endpoint [30];
        		sprintf (endpoint, "tcp://%s:%d", ipaddress, ntohs (beacon.port));
    			printf("Node: received beacon from ip %s which offers endpoint %s\n", ipaddress, endpoint);
			zsock_t *peer = zsock_new_dealer(endpoint);
			zstr_send(peer, "{'logger','tcp://'}");
		}
    		zuuid_destroy (&uuid);
		zstr_free (&ipaddress);
	} else if(which == inbox) {
		zmsg_t *msg = zmsg_recv(which);
		zmsg_print(msg);
		zmsg_destroy(&msg);
	}
    }
}
zpoller_destroy (&poller);

//  Stop listening
zstr_sendx (node1, "UNSUBSCRIBE", NULL);

//  Stop all node broadcasts
zstr_sendx (node1, "SILENCE", NULL);

//  Destroy the test nodes
zactor_destroy (&node);

 //struct timespec tstart={0,0}, tend={0,0};
 //   clock_gettime(CLOCK_MONOTONIC, &tstart);
 //   clock_gettime(CLOCK_MONOTONIC, &tend);
 //   printf("some_long_computation took about %.5f seconds\n",
 //          ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - 
 //          ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));
return 0;
}
