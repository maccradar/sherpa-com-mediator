#include <zyre.h>
#include <jansson.h>

typedef struct _json_msg_t {
    char *metamodel;
    char *model;
    char *type;
    char *payload;
} json_msg_t;

void decode_json(char* message, json_msg_t *result) {
    json_t *root;
    json_error_t error;
    root = json_loads(message, 0, &error);
    
    if(!root) {
   	printf("Error parsing JSON string! line %d: %s\n", error.line, error.text);
    	return;
    }

    result->metamodel = strdup(json_string_value(json_object_get(root, "metamodel")));
    result->model = strdup(json_string_value(json_object_get(root, "model")));
    result->type = strdup(json_string_value(json_object_get(root, "type")));
    result->payload = strdup(json_string_value(json_object_get(root, "payload")));

    json_decref(root);
}

int main(int argc, char *argv[]) {
    char *self = argv[1];
    char *hub = argv[2];
    bool verbose = true;    
    int major, minor, patch;
    zyre_version (&major, &minor, &patch);
    assert (major == ZYRE_VERSION_MAJOR);
    assert (minor == ZYRE_VERSION_MINOR);
    assert (patch == ZYRE_VERSION_PATCH);
    
    //  Create two nodes: 
    //  - local gossip node for backend
    //  - remote udp node for frontend
    zyre_t *local = zyre_new (self); // local
    assert (local);
    
    zyre_set_header(local,"type", "proxy");
    zyre_set_verbose (local);
    
    int rc = zyre_set_endpoint (local, "ipc://%s-local", self);
    assert (rc == 0);
    //  Set up gossip network for this node
    zyre_gossip_bind (local, "ipc://%s-hub", hub);
    rc = zyre_start (local);
    assert (rc == 0);

    zyre_t *remote = zyre_new (self);
    assert (remote);
    
    zyre_set_header(remote,"type", "proxy");
    zyre_set_verbose (remote);
    
    rc = zyre_start (remote);
    assert (rc == 0);
    assert (strneq (zyre_uuid (local), zyre_uuid (remote)));
    
    zyre_join (local, "SHERPA");
    zyre_join (remote, "SHERPALKU");

    //  Give time for them to interconnect
    zclock_sleep (100);
    if (verbose)
        zyre_dump (local);
    
    zpoller_t *poller =  zpoller_new (zyre_socket(local), zyre_socket(remote), NULL);
    while(!zsys_interrupted) {
	void *which = zpoller_wait (poller, ZMQ_POLL_MSEC);
        if (which == zyre_socket (local)) {
            printf("[%s] local data received!\n", self);
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self); 
	        return;
            }

            char *event = zmsg_popstr (msg);
            if (streq (event, "ENTER")) {
                assert (zmsg_size(msg) == 4);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                zframe_t *headers_packed = zmsg_pop (msg);
                char *address = zmsg_popstr (msg);

                printf ("[%s] %s %s %s <headers> %s\n", self, event, peerid, name, address);
                char* type = zyre_peer_header_value(remote, peerid, "type");
                printf ("[%s] %s has type %s\n",self, name, type);
                zstr_free(&peerid);
                zstr_free(&name);
                zframe_destroy(&headers_packed);
                zstr_free(&address);            
                zstr_free(&type);
            }
            else if (streq (event, "EXIT") || streq (event, "STOP")) {
                assert (zmsg_size(msg) == 2);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                printf ("[%s] %s %s %s\n", self, event, peerid, name);
                zstr_free(&peerid);
                zstr_free(&name);
            }
            else if (streq (event, "SHOUT")) {
                assert (zmsg_size(msg) == 4);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *group = zmsg_popstr (msg);
                char *message = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s %s\n", self, event, peerid, name, group, message);
                json_msg_t *result = (json_msg_t *) zmalloc (sizeof (json_msg_t));
                decode_json(message, result);
                printf ("[%s] message type %s\n", self, result->type);
		zyre_shouts(remote, "SHERPALKU", "%s", message);
                zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&group);
            }
            else if (streq (event, "JOIN")) {
                assert (zmsg_size(msg) == 3);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *group = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s\n", self, event, peerid, name, group);
                zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&group);
            } 
            else {
                zmsg_print(msg);
            }

            zstr_free (&event);
            zmsg_destroy (&msg);
        }	
        else if (which == zyre_socket (remote)) {
            printf("[%s] remote data received!\n", self);
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self); 
	        return;
            }

            char *event = zmsg_popstr (msg);
            if (streq (event, "ENTER")) {
                assert (zmsg_size(msg) == 4);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                zframe_t *headers_packed = zmsg_pop (msg);
                char *address = zmsg_popstr (msg);

                printf ("[%s] %s %s %s <headers> %s\n", self, event, peerid, name, address);
                char* type = zyre_peer_header_value(remote, peerid, "type");
                printf ("[%s] %s has type %s\n",self, name, type);
                zstr_free(&peerid);
                zstr_free(&name);
                zframe_destroy(&headers_packed);
                zstr_free(&address);            
                zstr_free(&type);
            }
            else if (streq (event, "EXIT") || streq (event, "STOP")) {
                assert (zmsg_size(msg) == 2);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                printf ("[%s] %s %s %s\n", self, event, peerid, name);
                zstr_free(&peerid);
                zstr_free(&name);
            }
            else if (streq (event, "SHOUT")) {
                assert (zmsg_size(msg) == 4);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *group = zmsg_popstr (msg);
                char *message = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s %s\n", self, event, peerid, name, group, message);
                zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&group);
            }
            else if (streq (event, "JOIN")) {
                assert (zmsg_size(msg) == 3);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *group = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s\n", self, event, peerid, name, group);
                zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&group);
            } 
            else {
                zmsg_print(msg);
            }

            zstr_free (&event);
            zmsg_destroy (&msg);
        }

    }
    zpoller_destroy (&poller);

    zyre_stop (remote);
    zyre_stop (local);

    zyre_destroy (&remote);
    zyre_destroy (&local);
    //  @end
    printf ("OK\n");
    return 0;
}
