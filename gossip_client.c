#include <zyre.h>
#include <jansson.h>

typedef struct _json_msg_t {
    char *metamodel;
    char *model;
    char *type;
    char *payload;
} json_msg_t;

static char* generate_json_msg(json_msg_t* m) {
  json_t *root = json_object();
  json_object_set_new( root, "metamodel", json_string(m->metamodel));
  json_object_set_new( root, "model", json_string(m->model));
  json_object_set_new( root, "type", json_string(m->type));
  json_object_set_new( root, "payload", json_string(m->payload));
  
  char* ret_strings = json_dumps( root, 0 );
  json_decref(root);
  return ret_strings;
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
    zyre_t *local = zyre_new (self); // local
    assert (local);
    
    zyre_set_header(local,"type", "roszyre");
    zyre_set_verbose (local);
    
    int rc;
    rc = zyre_set_endpoint (local, "ipc:///tmp/%s-local", self);
    assert (rc == 0);
    //  Set up gossip network for this node
    zyre_gossip_connect (local, "ipc:///tmp/%s-hub", hub);
    rc = zyre_start (local);
    assert (rc == 0);

    zyre_join (local, "local");

    if (verbose)
        zyre_dump (local);
    
    zpoller_t *poller =  zpoller_new (zyre_socket(local), NULL);
    while(!zsys_interrupted) {
	void *which = zpoller_wait (poller, ZMQ_POLL_MSEC);
        if (which == zyre_socket (local)) {
            printf("[%s] local data received!\n", self);
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self); 
	        return -1;
            }

            char *event = zmsg_popstr (msg);
            if (streq (event, "ENTER")) {
                assert (zmsg_size(msg) == 4);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                zframe_t *headers_packed = zmsg_pop (msg);
                char *address = zmsg_popstr (msg);

                printf ("[%s] %s %s %s <headers> %s\n", self, event, peerid, name, address);
                char* type = zyre_peer_header_value(local, peerid, "type");
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

    zyre_stop (local);

    zyre_destroy (&local);
    //  @end
    printf ("OK\n");
    return 0;
}
