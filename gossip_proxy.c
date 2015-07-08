#include <zyre.h>
#include <jansson.h>

typedef struct _json_payload_t {
  char *type;
  char *language;
  char *content;
} json_payload_t;

typedef struct _json_msg_t {
    char *metamodel;
    char *model;
    char *type;
//    json_payload_t payload;
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
    result->payload = strdup(json_dumps(json_object_get(root, "payload"), JSON_ENCODE_ANY));
    
    json_decref(root);
}

void send_join(zyre_t *remote, const char *peerid, json_t *payload) {
    json_t *root;
    root = json_object(); 
    json_object_set(root, "metamodel", json_string("sherpa_mgs"));
    json_object_set(root, "model", json_string("http://kul/join-team.json"));
    json_object_set(root, "type", json_string("join-team"));
    json_object_set(root, "payload", payload);
    zyre_whispers(remote, peerid, "%s", json_dumps(root, JSON_ENCODE_ANY));
}

void create_team(zyre_t *remote, char *payload) {
    json_error_t error;
    json_t *root = json_loads(payload, 0, &error);
    if(root) {
        json_t *members = json_object_get(root,"members");
	size_t index;
	json_t *value;

	json_array_foreach(members, index, value) {
    	/* block of code that uses index and value */
	      send_join(remote, json_string_value(value), json_object_get(root, "team"));
	}
    }
}

char* generate_peers(zyre_t *remote, json_t *config) {
    json_t *root;
    root = json_object(); 
    json_object_set(root, "metamodel", json_string("sherpa_mgs"));
    json_object_set(root, "model", json_string("http://kul/peer-list.json"));
    json_object_set(root, "type", json_string("peer-list"));
    json_t *payload = json_array();
    json_object_set(root, "payload", payload);
     
    zlist_t * peers = zyre_peers(remote);
    char *peer = zlist_first (peers);
    while(peer != NULL) {
        /* config is a JSON object */ 
        const char *key;
        json_t *value;
        json_t *headers = json_object();
        json_object_foreach(config, key, value) {
            /* block of code that uses key and value */
            char * header_value = zyre_peer_header_value(remote, peer, key);
            // Try to parse an array
	    json_error_t error;
	    json_t *header = json_loads(header_value,0,&error);
	    if(!header) {
	    	header = json_string(header_value);
            }
	    json_object_set(headers, key, header);
        }
        json_array_append(payload, headers);
        peer = zlist_next (peers);
    }
    return json_dumps(root, JSON_ENCODE_ANY);
}
json_t * load_config_file(char* file) {
    json_error_t error;
    json_t * root;
    root = json_load_file(file, JSON_ENSURE_ASCII, &error);
    printf("[%s] config file: %s\n", json_string_value(json_object_get(root, "short-name")), json_dumps(root, JSON_ENCODE_ANY));
    if(!root) {
   	printf("Error parsing JSON file! line %d: %s\n", error.line, error.text);
    	return NULL;
    }
    return root;	
}

int main(int argc, char *argv[]) {
    //char *self = argv[1];
    //char *hub = argv[2];
        
    int major, minor, patch;
    zyre_version (&major, &minor, &patch);
    assert (major == ZYRE_VERSION_MAJOR);
    assert (minor == ZYRE_VERSION_MINOR);
    assert (patch == ZYRE_VERSION_PATCH);
    
    // load configuration file
    json_t * config = load_config_file(argv[1]);
    const char *self = json_string_value(json_object_get(config, "short-name"));
    bool verbose = json_is_true(json_object_get(config, "verbose"));
    //  Create two nodes: 
    //  - local gossip node for backend
    //  - remote udp node for frontend
    zyre_t *local = zyre_new (self);
    assert (local); 
    zyre_t *remote = zyre_new (self);
    assert (remote);
    
    /* config is a JSON object */ 
    const char *key;
    json_t *value;
    json_object_foreach(config, key, value) {
        /* block of code that uses key and value */
	const char *header_value;
	if(json_is_string(value)) {
		header_value = json_string_value(value);
	} else {
		header_value = json_dumps(value, JSON_ENCODE_ANY);
	}	
	zyre_set_header(local, key, "%s", header_value);
        zyre_set_header(remote, key, "%s", header_value);
    }
 
    if(verbose)
	zyre_set_verbose (local);
    
    int rc;
    if(json_is_true(json_object_get(config, "gossip"))) {
    	rc = zyre_set_endpoint (local, "ipc:///tmp/%s-local", self);
    	assert (rc == 0);
    	//  Set up gossip network for this node
    	zyre_gossip_bind (local, "ipc:///tmp/%s-hub", self);
    }
    rc = zyre_start (local);
    assert (rc == 0);
    
    zyre_set_verbose (remote);
    
    rc = zyre_start (remote);
    assert (rc == 0);
    assert (strneq (zyre_uuid (local), zyre_uuid (remote)));
    char* localgroup = "local";
    char* remotegroup = "remote";
    zyre_join (local, localgroup);
    zyre_join (remote, remotegroup);

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
                if (streq (result->type, "peers")) {
			char *peerlist = generate_peers(remote, config);
			zyre_whispers(local, peerid, "%s", peerlist);
			printf ("[%s] sent peerlist to %s as reply to peers message: %s\n", self, name, peerlist);
			zstr_free(&peerlist);
		} else if (streq (result->type, "forward-all")) {
			zyre_shouts(remote, remotegroup, "%s", message);        
		} else if (streq (result->type, "forward")) {
                } else if (streq (result->type, "create-team")) {
			create_team(remote, result->payload);
		}
                zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&group);
            }
            else if (streq (event, "WHISPER")) {
                assert (zmsg_size(msg) == 3);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *message = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s\n", self, event, peerid, name, message);
                json_msg_t *result = (json_msg_t *) zmalloc (sizeof (json_msg_t));
                decode_json(message, result);
                printf ("[%s] message type %s\n", self, result->type);
		zyre_shouts(remote, "", "%s", message);        
                if (streq (result->type, "peers")) {
			char *peerlist = generate_peers(remote, config);
			zyre_whispers(local, peerid, "%s", peerlist);
			printf ("[%s] sent peerlist to %s as reply to peers message: %s\n", self, name, peerlist);
			zstr_free(&peerlist);
		} else if (streq (result->type, "create-team"))
			create_team(remote, result->payload);
                zstr_free(&peerid);
                zstr_free(&name);
            }
            else if (streq (event, "JOIN")) {
                assert (zmsg_size(msg) == 3);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *group = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s\n", self, event, peerid, name, group);
                // Welcome localgroup peer by sending him a peer list
		if(streq(group, localgroup)) {
			char *peerlist = generate_peers(remote, config);
			zyre_whispers(local, peerid, "%s", peerlist);
                	printf ("[%s] Welcomed %s by sending peer list: %s\n", self, name, peerlist);
                	zstr_free(&peerlist);
		}
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
	        // Update local group with new peer list
                char *peerlist = generate_peers(remote, config);
		zyre_shouts(local, localgroup, "%s", peerlist);
		printf ("[%s] sent updated peerlist to local group: %s\n", self, peerlist);
		zstr_free(&peerlist);
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
	        // Update local group with new peer list
                char *peerlist = generate_peers(remote, config);
		zyre_shouts(local, localgroup, "%s", peerlist);
		printf ("[%s] sent updated peerlist to local group: %s\n", self, peerlist);
		zstr_free(&peerlist);
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
                if(streq(result->type, "forward-all")) {
			zyre_shouts(local, localgroup, "%s", result->payload);
		}
		zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&group);
            }
            else if (streq (event, "WHISPER")) {
                assert (zmsg_size(msg) == 3);
                char *peerid = zmsg_popstr (msg);
                char *name = zmsg_popstr (msg);
                char *message = zmsg_popstr (msg);
                printf ("[%s] %s %s %s %s\n", self, event, peerid, name, message);
                json_msg_t *result = (json_msg_t *) zmalloc (sizeof (json_msg_t));
                decode_json(message, result);
                printf ("[%s] message type %s\n", self, result->type);
                if(streq(result->type, "join-team")) {
                    printf("[%s] joining team %s\n", self, result->payload);
		    zyre_join(remote, result->payload);
		}
 	        zstr_free(&peerid);
                zstr_free(&name);
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
