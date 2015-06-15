//  --------------------------------------------------------------------------
//  Zyre is Copyright (c) 2010-2014 iMatix Corporation and Contributors
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  --------------------------------------------------------------------------
#include <jansson.h>
#include <time.h>
#include <zyre.h>

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

static void observer_actor (zsock_t *pipe, void *args) {
    char** argv = (char**) args;
    char* name = (char*) argv[0];
    char* group = (char*) argv[1];
    char* file = (char*) argv[2];
    zyre_t *node = zyre_new(name);
    if (!node)
 	return;
    zyre_start(node);
    zyre_join(node, group);
    zsock_signal(pipe,0);
	
    bool terminated = false;
    zpoller_t *poller = zpoller_new (pipe, zyre_socket (node), NULL);
    while (!terminated) {
        void *which = zpoller_wait (poller, -1); // no timeout
        if (which == pipe){
            zmsg_t *msg = zmsg_recv (which);
            if (!msg)
                break;              //  Interrupted
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")) {
                terminated = true;
            }
            else
            if (streq (command, "SHOUT")) { 
                char *string = zmsg_popstr (msg);
                zyre_shouts (node, group, "%s", string);
            }
            else {
                puts ("E: invalid message to actor");
                assert (false);
            }
            free (command);
            zmsg_destroy (&msg);
        }
        else if (which == zyre_socket (node)) {
            zmsg_t *msg = zmsg_recv (which);
            char *event_id = zmsg_popstr (msg);
            char *peer_id = zmsg_popstr (msg);
            char *name_id = zmsg_popstr (msg);
            char *group_id = zmsg_popstr (msg);
            char *message_id = zmsg_popstr (msg);

            if (streq (event_id, "ENTER")) { 
                printf ("%s has joined the chat\n", name_id);
            }
            else if (streq (event_id, "EXIT")) { 
                printf ("%s has left the chat\n", name_id);
            }
            if (streq (event_id, "SHOUT")) { 
                printf ("%s: %s\n", name_id, message_id);
            }
            printf ("Message from node\n");
            printf ("event: %s peer: %s  name: %s\n  group: %s message: %s\n", event_id, peer_id, name_id, group_id, message_id);

            free (event_id);
            free (peer_id);
            free (name_id);
            free (group_id);
            free (message_id);
            zmsg_destroy (&msg);
        }
    }
    zpoller_destroy (&poller);

    // Notify peers that this peer is shutting down. Provide
    // a brief interval to ensure message is emitted.
    zyre_stop(node);
    zclock_sleep(100);

    zyre_destroy (&node);
}



int
main (int argc, char *argv[])
{
    if (argc < 4) {
        puts ("syntax: ./observer name group file");
        exit (0);
    }
    zactor_t *actor = zactor_new (observer_actor, argv);
    assert (actor);
    
    while (!zsys_interrupted) {
        char message [1024];
        if (!fgets( message, 1024, stdin))
	        break;
	    message[strlen(message)-1] = 0; // drop the trailing linefeed
	    zstr_sendx (actor, "SHOUT", message, NULL);
    }

    zactor_destroy (&actor);

    return 0;
}
/*
int main(void) {
    bool verbose = true;
    int major, minor, patch;
    zyre_version (&major, &minor, &patch);
    assert (major == ZYRE_VERSION_MAJOR);
    assert (minor == ZYRE_VERSION_MINOR);
    assert (patch == ZYRE_VERSION_PATCH);
    
    zsock_t* collector_sub = zsock_new_sub("inproc://collector", "*");
 
    //  Create two nodes
    zyre_t * collector = zyre_new ("collector");
    assert (collector);
    assert (streq (zyre_name (collector), "collector"));
    zyre_set_header (collector, "X-HELLO", "collector");
    zyre_set_header (collector, "X-COLLECTOR", "inproc://collector");
    if (verbose)
        zyre_set_verbose (collector);
    
    //  Set inproc endpoint for this node
    int rc = zyre_set_endpoint (collector, "inproc://zyre-collector");
    assert (rc == 0);
    //  Set up gossip network for this node
    zyre_gossip_bind (collector, "inproc://sherpa-hub");
    rc = zyre_start (collector);
    assert (rc == 0);

    zyre_t *node2 = zyre_new ("node2");
    assert (node2);
    assert (streq (zyre_name (node2), "node2"));
    if (verbose)
        zyre_set_verbose (node2);
    
    //  Set inproc endpoint for this node
    rc = zyre_set_endpoint (node2, "inproc://zyre-node2");
    assert (rc == 0);
   
    //  Set up gossip network for this node
    zyre_gossip_connect (node2, "inproc://sherpa-hub");
    rc = zyre_start (node2);
    assert (rc == 0);
    assert (strneq (zyre_uuid (collector), zyre_uuid (node2)));
    
    zyre_join (collector, "SHERPA");
    zyre_join (node2, "SHERPA");

    //  Give time for them to interconnect
    zclock_sleep (100);
    if (verbose)
        zyre_dump (collector);

    zlist_t *peers = zyre_peers (collector);
    assert (peers);
    assert (zlist_size (peers) == 1);
    zlist_destroy (&peers);

    char *value = zyre_peer_header_value (node2, zyre_uuid (collector), "X-HELLO");
    assert (streq (value, "collector"));
    zstr_free (&value);

    //  One node shouts to GLOBAL
    //zyre_shouts (collector, "GLOBAL", "Hello, World");

    //  Second node should receive ENTER, JOIN, and SHOUT
    zmsg_t *msg = zyre_recv (node2);
    assert (msg);
    char *command = zmsg_popstr (msg);
//    assert (streq (command, "ENTER"));
    printf("Node 2 received %s\n", command);
    zmsg_print(msg);
    zstr_free (&command);
    
    assert (zmsg_size (msg) == 4);
    char *peerid = zmsg_popstr (msg);
    char *name = zmsg_popstr (msg);
    assert (streq (name, "collector"));
    zstr_free (&name);
    zframe_t *headers_packed = zmsg_pop (msg);
    
    char *address = zmsg_popstr (msg);
    char *endpoint = zyre_peer_address (node2, peerid);
    assert (streq (address, endpoint));
    zstr_free (&peerid);
    zstr_free (&endpoint);
    zstr_free (&address);

    assert (headers_packed);
    zhash_t *headers = zhash_unpack (headers_packed);
    assert (headers);
    zframe_destroy (&headers_packed);
    assert (streq ((char *) zhash_lookup (headers, "X-HELLO"), "collector"));
    assert (streq ((char *) zhash_lookup (headers, "X-LOGGER"), "inproc://logger"));
    zhash_destroy (&headers);
    zmsg_destroy (&msg);
    
    msg = zyre_recv (node2);
    assert (msg);
    command = zmsg_popstr (msg);
    //assert (streq (command, "JOIN"));
    printf("Node 2 received %s\n", command);
    zmsg_print(msg);
    zstr_free (&command);
    assert (zmsg_size (msg) == 3);
    zmsg_destroy (&msg);

    msg = zyre_recv (node2);
    assert (msg);
    command = zmsg_popstr (msg);
    //assert (streq (command, "JOIN"));
    printf("Node 2 received %s\n", command);
    zmsg_print(msg);
    zstr_free (&command);
    assert (zmsg_size (msg) == 3);
    zmsg_destroy (&msg);

    msg = zyre_recv (node2);
    assert (msg);
    command = zmsg_popstr (msg);
    printf("Node 2 received %s\n", command);
    zmsg_print(msg);
    //assert (streq (command, "SHOUT"));
    zstr_free (&command);
    zmsg_destroy (&msg);

    zyre_stop (node2);

    msg = zyre_recv (node2);
    assert (msg);
    command = zmsg_popstr (msg);
    //assert (streq (command, "STOP"));
    printf("Node 2 received %s\n", command);
    zmsg_print(msg);
    zstr_free (&command);
    zmsg_destroy (&msg);

    zyre_stop (collector);

    zyre_destroy (&collector);
    zyre_destroy (&node2);
    //  @end
    printf ("OK\n");


char* hb = generate_json_heartbeat("collector");
zsock_t *inbox = zsock_new (ZMQ_ROUTER);
// connect monitor to the inbox
zactor_t *qosmon = zactor_new (zmonitor, inbox);
assert (qosmon);
zstr_sendx (qosmon, "VERBOSE", NULL);
zstr_sendx (qosmon, "LISTEN", "ALL", NULL);
zstr_sendx (qosmon, "START", NULL);
zsock_wait (qosmon);

zactor_t *node = zactor_new (zbeacon, NULL);
assert (collector);
zsock_send (node, "si", "CONFIGURE", 5670);
char* hostname = zstr_recv (collector);
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
zstr_sendx (collector, "UNSUBSCRIBE", NULL);

//  Stop all node broadcasts
zstr_sendx (collector, "SILENCE", NULL);

//  Destroy the test nodes
zactor_destroy (&node);

 //struct timespec tstart={0,0}, tend={0,0};
 //   clock_gettime(CLOCK_MONOTONIC, &tstart);
 //   clock_gettime(CLOCK_MONOTONIC, &tend);
 //   printf("some_long_computation took about %.5f seconds\n",
 //          ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - 
 //          ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));
return 0;
}*/
