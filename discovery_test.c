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

zactor_t *node1 = zactor_new (zbeacon, NULL);
assert (node1);
zsock_send (node1, "si", "CONFIGURE", 5670);
char* hostname = zstr_recv (node1);
assert (*hostname);
int port = zsock_bind (inbox, "tcp://%s:*", hostname);
printf("Node 1: listening on %s:%i\n", hostname, port);
free (hostname);

beacon_t beacon1;
beacon1.protocol [0] = 'S';
beacon1.protocol [1] = 'H';
beacon1.protocol [2] = 'E';
beacon1.protocol [3] = 'R';
beacon1.protocol [4] = 'P';
beacon1.protocol [5] = 'A';
beacon1.version = BEACON_VERSION;
beacon1.port = htons (port);
zuuid_t *uuid1 = zuuid_new();
zuuid_export (uuid1, beacon1.uuid);


zactor_t *node2 = zactor_new (zbeacon, NULL);
assert (node2);
zsock_send (node2, "si", "CONFIGURE", 5670);
hostname = zstr_recv (node2);
printf("Node 2: hostname %s\n", hostname);
assert (*hostname);
free (hostname);

zactor_t *node3 = zactor_new (zbeacon, NULL);
assert (node3);
zsock_send (node3, "si", "CONFIGURE", 5670);
hostname = zstr_recv (node3);
printf("Node 3: hostname %s\n", hostname);
assert (*hostname);
free (hostname);

zsock_send (node1, "sbi", "PUBLISH", (byte *) &beacon1, sizeof(beacon_t), 1000);
printf("Node 1: publishing sherpa beacon\n");
zsock_send (node2, "sbi", "PUBLISH", hb, strlen(hb), 1000);
printf("Node 2: publishing json heartbeat\n");
zsock_send (node3, "sbi", "PUBLISH", "NODE/3", 6, 500);
printf("Node 3: publishing string\n");
zsock_send (node1, "sb", "SUBSCRIBE", "NODE", 4);
printf("Node 1: filtering on NODE beacons\n");
zsock_send (node2, "sb", "SUBSCRIBE", "SHERPA", 6);
printf("Node 2: filtering on SHERPA beacons\n");
zsock_send (node3, "sb", "SUBSCRIBE", "", 0);
printf("Node 3: no filtering\n");


//  Poll on three API sockets at once
zpoller_t *poller = zpoller_new (node1, node2, node3, inbox, NULL);
assert (poller);
int64_t stop_at = zclock_mono () + 5000;
while (zclock_mono () < stop_at) {
    long timeout = (long) (stop_at - zclock_mono ());
    if (timeout < 0)
        timeout = 0;
    void *which = zpoller_wait (poller, timeout * ZMQ_POLL_MSEC);
    if (which) {
	int i = 0;
	if(which == node1) {
		i = 1;
        	char *ipaddress, *received;
        	zstr_recvx (which, &ipaddress, &received, NULL);
        	printf("Node %i: received from ip %s: %s\n", i, ipaddress, received);
		zstr_free (&ipaddress);
        	zstr_free (&received);
	} else if(which == node2) {
		i = 2;
        	char *ipaddress = zstr_recv (which);
        	printf("Node %i: received from ip %s\n", i, ipaddress);
    		zframe_t *frame = zframe_recv (which);
    		if (ipaddress == NULL) {
        		printf("Node %i: got interrupted\n", i);                 //  Interrupted
			break;
		}
    		//  Ignore anything that isn't a valid beacon
    		beacon_t beacon = { { 0 } };
    		if (zframe_size (frame) == sizeof (beacon_t))
        		memcpy (&beacon, zframe_data (frame), zframe_size (frame));
    		zframe_destroy (&frame);
    		if (beacon.version != BEACON_VERSION) {
        		printf("Node %i: Invalid beacon version\n", i);                 //  Garbage beacon, ignore it
			break;
		}
    		zuuid_t *uuid = zuuid_new ();
    		zuuid_set (uuid, beacon.uuid);
    		if (beacon.port) {
        		char endpoint [30];
        		sprintf (endpoint, "tcp://%s:%d", ipaddress, ntohs (beacon.port));
    			printf("Node %i: received beacon from ip %s which offers endpoint %s\n", i, ipaddress, endpoint);
			zsock_t *worker = zsock_new_req(endpoint);
			zstr_send(worker, "HELLO!");			
		}
    		zuuid_destroy (&uuid);
		zstr_free (&ipaddress);
	} else if(which == node3) {
		i = 3;
        	char *ipaddress, *received;
        	zstr_recvx (which, &ipaddress, &received, NULL);
        	printf("Node %i: received from ip %s: %s\n", i, ipaddress, received);
		zstr_free (&ipaddress);
        	zstr_free (&received);
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
zstr_sendx (node2, "SILENCE", NULL);
zstr_sendx (node3, "SILENCE", NULL);

//  Destroy the test nodes
zactor_destroy (&node1);
zactor_destroy (&node2);
zactor_destroy (&node3);


 //struct timespec tstart={0,0}, tend={0,0};
 //   clock_gettime(CLOCK_MONOTONIC, &tstart);
 //   clock_gettime(CLOCK_MONOTONIC, &tend);
 //   printf("some_long_computation took about %.5f seconds\n",
 //          ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - 
 //          ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));

//zsock_t *socket = zsock_new (ZMQ_DEALER);
//assert (socket);

//zsock_bind (socket, "tcp://*:9000");

/*zpoller_t* poller =  zpoller_new (socket, qosmon, NULL);
while(true) {
	void *which = zpoller_wait (poller, 0);
	if (which == socket) {
            zmsg_t *msg = zmsg_recv (which);
	    char *str = zmsg_popstr(msg);
	    
	    free(str);
            zmsg_destroy(&msg);
        }
	if (which == qosmon) {
	    zmsg_t *msg = zmsg_recv(qosmon);
	    char *event = zmsg_popstr(msg);
            printf("Server monitor: %s\n", event);
 	    free(event);
	    zmsg_destroy(&msg);
	}

}
*/
return 0;
}
