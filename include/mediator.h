#ifndef MEDIATOR_H
#define MEDIATOR_H

#include <zyre.h>
#include <jansson.h>
#include <errno.h>
#include <sys/stat.h>
//#include <loglevels.h>

typedef struct _mediator_t {
    const char *shortname;
    const char *localgroup;
    const char *remotegroup;
    zyre_t *local;
    zyre_t *remote;
    json_t *config;
    zlist_t *filter_list;
    zlist_t *send_msgs;
    bool verbose;
    zpoller_t *poller;
    zhash_t *queries;
    zlist_t *remote_query_list;
    zlist_t *local_query_list;
    const char* actor_timeout;
} mediator_t;

typedef struct _json_msg_t {
    char *metamodel;
    char *model;
    char *type;
    char *payload;
} json_msg_t;

typedef struct _recipient_t {
	const char *id;
	bool ack;
} recipient_t;

typedef struct _filter_list_item_t {
	const char *sender;
	const char *msg_UID;
	int64_t ts;
}filter_list_item_t;

typedef struct _send_msg_request_t {
	const char *uid;
	const char *local_requester;
	const char* group;
        int64_t ts_added;
	int64_t ts_last_sent;
	int timeout; // in msec
	zlist_t *recipients;
	const char *payload_type;
	const char *msg; // payload+metadata
} send_msg_request_t;

typedef struct _query_t {
        const char *uid;
        const char *requester;
        json_msg_t *msg;
        zactor_t *loop;
} query_t;

void mediator_destroy (mediator_t **self_p) {
    assert (self_p);
    if(*self_p) {
        mediator_t *self = *self_p;
        zyre_destroy (&self->local);
        zyre_destroy (&self->remote);
        zlist_destroy (&self->send_msgs);
        zlist_destroy (&self->filter_list);
	zlist_destroy (&self->remote_query_list);
 	zlist_destroy (&self->local_query_list);
	zhash_destroy (&self->queries);
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}

mediator_t * mediator_new (json_t *config) {
    mediator_t *self = (mediator_t *) zmalloc (sizeof (mediator_t));
    if (!self)
        return NULL;
    
    int major, minor, patch;
    zyre_version (&major, &minor, &patch);
    if (major != ZYRE_VERSION_MAJOR)
        return NULL;
    if (minor != ZYRE_VERSION_MINOR)
        return NULL;
    if (patch != ZYRE_VERSION_PATCH)
        return NULL;
    self->config = config;
    //init list of send msg requests
    self->send_msgs = zlist_new();
    if (!self->send_msgs) {
        mediator_destroy (&self);
        return NULL;
    }

    //init list for filtering msg requests
    self->filter_list = zlist_new();
    if (!self->filter_list) {
        mediator_destroy (&self);
        return NULL;
    }

    //init list for remote queries
    self->remote_query_list = zlist_new();
    if (!self->remote_query_list) {
        mediator_destroy (&self);
        return NULL;
    }
    
    //init list for local queries
    self->local_query_list = zlist_new();
    if (!self->local_query_list) {
        mediator_destroy (&self);
        return NULL;
    }

    if (json_object_get(config, "short-name")) {
    	self->shortname = json_string_value(json_object_get(config, "short-name"));
	} else {
		printf("No shortname given.\n");
		return NULL;
	}

    if (json_object_get(config, "verbose")) {
    	self->verbose = json_is_true(json_object_get(config, "verbose"));
	} else {
		self->verbose = false;
	}

    self->queries = zhash_new();
    if (!self->queries) {
        mediator_destroy (&self);
        return NULL;
    }
 
    //  Create two nodes: 
    //  - local gossip node for backend
    //  - remote udp node for frontend
    self->local = zyre_new (self->shortname);
    if (!self->local) {
        mediator_destroy (&self);
        return NULL;
    }
    self->remote = zyre_new (self->shortname);
    if (!self->remote) {
        mediator_destroy (&self);
        return NULL;
    }

    printf("[%s] my remote UUID: %s\n", self->shortname, zyre_uuid(self->remote));
    json_object_set(config, "peerid", json_string(zyre_uuid(self->remote)));  
    /* config is a JSON object */
    // set values for config file as zyre header.
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
    	//printf(header_value);printf("\n");
    	zyre_set_header(self->local, key, "%s", header_value);
        zyre_set_header(self->remote, key, "%s", header_value);
    }
 
    if(self->verbose) {
    	zyre_set_verbose (self->local);
        zyre_set_verbose (self->remote);
    }
    
    int rc;
    if(!json_is_null(json_object_get(config, "gossip_endpoint"))) {
    	if(!json_is_null(json_object_get(config, "local_endpoint"))) {
    		rc = zyre_set_endpoint (self->local, "%s", json_string_value(json_object_get(config, "local_endpoint")));
    		assert (rc == 0);
    	} else {
    		printf("[%s] WARNING: no local gossip endpoint is set! \n", self->shortname);
    	}

    	//  Set up gossip network for this node
    	zyre_gossip_bind (self->local, "%s", json_string_value(json_object_get(config, "gossip_endpoint")));
    	printf("[%s] using gossip with gossip hub '%s' \n", self->shortname,json_string_value(json_object_get(config, "gossip_endpoint")));
    } else {
    	printf("[%s] WARNING: no local gossip communication is set! \n", self->shortname);
    }
    rc = zyre_start (self->local);
    assert (rc == 0);
    rc = zyre_start (self->remote);
    assert (rc == 0);
    assert (strneq (zyre_uuid (self->local), zyre_uuid (self->remote)));
    
	const char* localgroup;
	if(!json_is_null(json_object_get(config, "local-network"))) {
		localgroup = json_string_value(json_object_get(config, "local-network"));
	} else {
		printf("[%s] WARNING: no name for local network set! Will use default name [local].",self->shortname);
		localgroup = "local";
	}
	const char* remotegroup;
	if(!json_is_null(json_object_get(config, "remote-network"))) {
		remotegroup = json_string_value(json_object_get(config, "remote-network"));
	} else {
		printf("[%s] WARNING: no name for local network set! Will use default name [local].",self->shortname);
		remotegroup = "remote";
	}
    zyre_join (self->local, localgroup);
    zyre_join (self->remote, remotegroup);
    self->localgroup = localgroup;
    self->remotegroup = remotegroup;
    //  Give time for them to interconnect
    zclock_sleep (100);
    if (self->verbose) {
        zyre_dump (self->local);
        zyre_dump (self->remote);
    }

    zpoller_t *poller =  zpoller_new (zyre_socket(self->local), zyre_socket(self->remote), NULL);
    self->poller = poller;

    zhash_t *hash = zhash_new ();
    if (!hash) {
        mediator_destroy (&self);
        return NULL;
    }

    if (json_object_get(config, "actor_timeout")) {
		self->actor_timeout = json_string_value(json_object_get(config, "actor_timeout"));
	} else {
		printf("No actor_timeout given, will use default 5s.\n");
		self->actor_timeout = strdup("5");
	}

    return self;
}


void query_destroy (query_t **self_p) {
        assert (self_p);
        if(*self_p) {
            query_t *self = *self_p;
            free (self);
            *self_p = NULL;
        }
}

query_t * query_new (const char *uid, const char *requester, json_msg_t *msg, zactor_t *loop) {
        query_t *self = (query_t *) zmalloc (sizeof (query_t));
        if (!self)
            return NULL;
        self->uid = uid;
        self->requester = requester;
        self->msg = msg;
        self->loop = loop;
        
        return self;
}


// File transfer protocol
#define CHUNK_SIZE 250000
#define PIPELINE   10

static void
client_actor (zsock_t *pipe, void *args)
{
    char* peerid = strdup(((char**)args)[0]);
    char* uid = strdup(((char**)args)[1]);
    char* endpoint = strdup(((char**)args)[2]);
    char* target = strdup(((char**)args)[3]);
    char* timeout_str = strdup(((char**)args)[4]);
    char* filesize = strdup(((char**)args)[5]);
    char *eptr;
    off_t fs = strtoll(filesize, &eptr, 10);
    assert (timeout_str);
    int timeout = atoi(timeout_str);

    char* success = NULL;
    char* error = NULL;

    // time when thread was spawned; will be updated whenever a message is received
    int64_t com_time = zclock_mono();

    zsock_t *dealer = zsock_new_dealer(endpoint);
    
    //  Up to this many chunks in transit
    size_t credit = PIPELINE;
    
    size_t total = 0;       //  Total bytes received
    size_t chunks = 0;      //  Total chunks received
    size_t offset = 0;      //  Offset of next chunk request
    
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    
    zpoller_t *poller = zpoller_new (pipe, dealer, NULL);

    FILE *file = fopen (target, "w");
	printf("[client_actor] peerid: %s\n",peerid);
	printf("[client_actor] uid: %s\n",uid);
	printf("[client_actor] endpoint: %s\n",endpoint);
	printf("[client_actor] storing file at %s\n",target);
	printf("[client_actor] timeout %d\n",timeout);
	printf("[client_actor] file size %s\n",filesize);
	if(!file) {
		printf("[client_actor] errno = %d\n", errno);
		printf("[client_actor] Check http://www.virtsync.com/c-error-codes-include-errno for explanation\n");
		printf ("[client_actor] Cannot open target file %s for file transfer: \n", target);
		success = strdup("false");
		error = strdup("[client_actor] Could not create file. Please check target folder.");
		goto cleanup;
	}

    while (!zsys_interrupted) {//!zsys_interrupted
        void *which = zpoller_wait (poller, 1);
        ///TODO: remove hardcoded timeout
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (which);
            if (!msg){
            	printf("[client_actor] Pipe interrupted.\n");
            	success = strdup("false");
				error = strdup("[client_actor] Pipe interrupted.");
				fclose(file);
				goto cleanup;              //  Interrupted
            }
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")){
            	printf("[client_actor] Received term signal.\n");
            	success = strdup("false");
				error = strdup("[client_actor] Received $TERM signal.");
				fclose(file);
				zmsg_destroy (&msg);
				goto cleanup;
            }
            zmsg_destroy (&msg);
        }
        else if (which == dealer) {
			zframe_t *chunk = zframe_recv (dealer);
			if (!chunk){
				printf("[client_actor] Dealer socket interrupted.\n");
				success = strdup("false");
				error = strdup("[client_actor] Dealer socket interrupted.");
				fclose(file);
				goto cleanup;
			}
			chunks++;
			credit++;
			com_time = zclock_mono(); // reset timeout when receiving a package
			size_t size = zframe_size (chunk);
			fwrite (zframe_data(chunk) , sizeof(char), size, file);
			zframe_destroy (&chunk);
			total += size;
			printf ("[client_actor] %zd chunks received, %zd bytes of %s\n", chunks, total, filesize);
			if (size < CHUNK_SIZE)
				break;              //  Last chunk received; exit
        }
        while (credit) {
			// Ask for next chunk
        	if (offset > fs) {
        		printf("[client_actor] offset larger than file size. Will not send fetch request.\n");
        		break;
        	}
			zstr_sendm  (dealer, "fetch");
			zstr_sendfm (dealer, "%ld", offset);
			zstr_sendf  (dealer, "%ld", (long) CHUNK_SIZE);
			printf ("[client_actor] Sending fetch request with offset %zu\n",offset);
			offset += CHUNK_SIZE;
			credit--;
		}
        // check for timeout
        int64_t curr_time = zclock_mono ();
		if (curr_time > 0) {
			//printf("time: %zu", (curr_time - com_time));
			if (curr_time - com_time > (1000 * timeout)) {
				success = strdup("false");
				error = strdup("[client_actor] Timeout.");
				printf("[client_actor] timeout!\n");
				fclose(file);
				///TODO:test
				goto cleanup;
			}
		} else {
			printf ("[client_actor] could not get current time\n");
		}
    }
    printf ("[client_actor] File transfer complete. Received %zd bytes\n", total);
    success = strdup("true");
    error = strdup("");
    fclose(file);
cleanup:
	printf ("[client_actor] Creating report\n");
	// Query type
    zstr_sendm (pipe, "remote_file_done");
    zstr_sendm (pipe, peerid);
    zstr_sendm (pipe, uid);
    zstr_sendm (pipe, success);
    zstr_sendm (pipe, error);
    zstr_send (pipe, target);

    zstr_free(&peerid);
    zstr_free(&uid);
    zstr_free(&endpoint);
    zstr_free(&target);
    zstr_free(&timeout_str);
    zstr_free(&filesize);
    zstr_free(&success);
    zstr_free(&error);
    
	printf ("[client_actor] Cleaning up client actor\n");
    zpoller_destroy(&poller);
    zsock_destroy(&dealer);
}

//  The server thread waits for a chunk request from a client,
//  reads that chunk and sends it back to the client:

static void
server_actor (zsock_t *pipe, void *args)
{
    // TODO: use JSON with parsed URI upfront?
    char* uri = strdup(((char**)args)[0]);
    char* token;
    token = strtok(uri, ":"); // Remove host/peerid
    token = strtok(NULL, ":");
    char* filename = strdup(token);

    char* success = NULL;
    char* error = NULL;

	char* uid = strdup(((char**)args)[2]);
	char* peerid = strdup(((char**)args)[3]);

	char* timeout_str = strdup(((char**)args)[1]);
	assert (timeout_str);
	int timeout = atoi(timeout_str);

    zsock_t *router = zsock_new_router ("tcp://*:*");
    assert(router);
	//  We have two parts per message so HWM is PIPELINE * 2
	zsocket_set_hwm (zsock_resolve(router), PIPELINE * 2);
	zpoller_t *poller = zpoller_new (pipe, router, NULL);
	assert(poller);

	zsock_signal (pipe, 0);     //  Signal "ready" to caller

    FILE *file = fopen (filename, "r");
    if(!file) {
		printf ("[server_actor] Cannot open target file %s for file transfer: \n", filename);
		printf("[server_actor] errno = %d\n", errno);
		printf("Check http://www.virtsync.com/c-error-codes-include-errno for explanation\n");
		success = strdup("false");
		error = strdup("[server_actor] Could not create file. Please check target folder.");
		// Query type
		zstr_sendm (pipe, "remote_file_transfer_error");
		zstr_sendm (pipe, peerid);
		zstr_sendm (pipe, uid);
		zstr_sendm (pipe, success);
		zstr_send (pipe, error);
		printf ("[server_actor] waiting for msg\n");
		int rc;
		rc = zsock_wait (pipe);
		if (rc == 123){
			printf("[server_actor] Received signal from pipe. Cleaning up %s.\n", zsock_endpoint(router));
		} else {
			printf("[server_actor] Received wrong signal from pipe. Cleaning up %s anyway.\n", zsock_endpoint(router));
		}
		goto cleanup;
	}
	// time when thread was spawned; will be updated whenever a message is received
	int64_t com_time = zclock_mono();

    // get file size
    off_t file_size;
	struct stat st;
	if (stat(filename, &st) == 0){
		file_size=st.st_size;
		printf("[server_actor] Opened file of size %zu.\n",file_size);
	}
	else {
		printf("[server_actor] could not determine file size. Errno: = %d\n",errno);
		success = strdup("false");
		error = strdup("[server_actor] could not determine file size.");
		fclose (file);
		// Query type
		zstr_sendm (pipe, "remote_file_transfer_error");
		zstr_sendm (pipe, peerid);
		zstr_sendm (pipe, uid);
		zstr_sendm (pipe, success);
		zstr_send (pipe, error);

		int rc;
		rc = zsock_wait (pipe);
		if (rc == 123){
			printf("[server_actor] Received signal from pipe. Cleaning up %s.\n", zsock_endpoint(router));
		} else {
			printf("[server_actor] Received wrong signal from pipe. Cleaning up %s anyway.\n", zsock_endpoint(router));
		}
		goto cleanup;
	}

    // Inform caller our endpoint
	zstr_send (pipe, zsock_endpoint(router));
	char file_size_str[20];
	sprintf(file_size_str, "%zu", file_size);
	zstr_send (pipe, file_size_str);

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, 1);
        ///TODO: replace hardcoded timeout
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (which);
            if (!msg){
            	success = strdup("false");
				error = strdup("[server_actor] pipe interrupted");
				fclose (file);
				// Query type
				zstr_sendm (pipe, "remote_file_transfer_error");
				zstr_sendm (pipe, peerid);
				zstr_sendm (pipe, uid);
				zstr_sendm (pipe, success);
				zstr_send (pipe, error);

				int rc;
				rc = zsock_wait (pipe);
				if (rc == 123){
					printf("[server_actor] Received signal from pipe. Cleaning up %s.\n", zsock_endpoint(router));
				} else {
					printf("[server_actor] Received wrong signal from pipe. Cleaning up %s anyway.\n", zsock_endpoint(router));
				}
				goto cleanup;
            }

            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")){
            	printf("[server_actor] Received term signal.\n");
            	success = strdup("false");
            	error = strdup("[server_actor] Received $TERM signal.");
				fclose (file);
				goto cleanup;
            }
            zmsg_destroy (&msg);
        } else if (which == router) {
        	// reset timeout timer
        	com_time = zclock_mono ();

            //  First frame in each message is the sender identity
            zframe_t *identity = zframe_recv (router);
            if (!identity){
            	success = strdup("false");
            	error = strdup("[server_actor] router interrupted");
            	fclose (file);
            	// Query type
				zstr_sendm (pipe, "remote_file_transfer_error");
				zstr_sendm (pipe, peerid);
				zstr_sendm (pipe, uid);
				zstr_sendm (pipe, success);
				zstr_send (pipe, error);

				int rc;
				rc = zsock_wait (pipe);
				if (rc == 123){
					printf("[server_actor] Received signal from pipe. Cleaning up %s.\n", zsock_endpoint(router));
				} else {
					printf("[server_actor] Received wrong signal from pipe. Cleaning up %s anyway.\n", zsock_endpoint(router));
				}
                goto cleanup;             //  Shutting down, quit
            }
            //zframe_print(identity,"identity frame: ");
            
            //  Second frame is "fetch" command
            char *command = zstr_recv (router);
            assert (streq (command, "fetch"));
            printf("[server_actor] Received fetch.\n");
            zstr_free(&command);

            //  Third frame is chunk offset in file
            char *offset_str = zstr_recv (router);
            assert (offset_str);
            size_t offset = atoi (offset_str);
            if (offset > file_size){
            	printf("[server_actor] Offset larger than file_size. Ignoring fetch request\n");
            } else {
				printf("[server_actor] Offset %zu in file_size %zu.\n",offset, file_size);
				zstr_free(&offset_str);

				//  Fourth frame is maximum chunk size
				char *chunksz_str = zstr_recv (router);
				assert (chunksz_str);
				size_t chunksz = atoi (chunksz_str);
				printf("[server_actor] chunk size %zu.\n",chunksz);
				zstr_free(&chunksz_str);

				//  Read chunk of data from file
				fseek (file, offset, SEEK_SET);
				byte *data = malloc (chunksz);
				assert (data);

				//  Send resulting chunk to client
				size_t size = fread (data, 1, chunksz, file);
				zframe_t *chunk = zframe_new (data, size);
				//  zframe_send destroys the frames automatically
				printf("[server_actor] Serving chunk\n");
				//zframe_print(identity,"identity frame: ");
				zframe_send (&identity, router, ZFRAME_MORE);
				zframe_send (&chunk, router, 0);
				free(data);
            }
        }
        // check for timeout
		int64_t curr_time = zclock_mono ();
		if (curr_time > 0) {
			if (curr_time - com_time > (1000 * timeout)) {
				///TODO: test
				printf("[client_actor] timeout!\n");
				success = strdup("false");
				error = strdup("[server_actor] Timeout");
				fclose (file);
				// Query type
				zstr_sendm (pipe, "remote_file_transfer_error");
				zstr_sendm (pipe, peerid);
				zstr_sendm (pipe, uid);
				zstr_sendm (pipe, success);
				zstr_send (pipe, error);
				int rc;
				rc = zsock_wait (pipe);
				if (rc == 123){
					printf("[server_actor] Received signal from pipe. Cleaning up %s.\n", zsock_endpoint(router));
				} else {
					printf("[server_actor] Received wrong signal from pipe. Cleaning up %s anyway.\n", zsock_endpoint(router));
				}
				goto cleanup;
			}
		} else {
			printf ("[client_actor] could not get current time\n");
		}
    }
    printf("Finished serving %s on %s\n", filename, zsock_endpoint(router));
    success = strdup("true");
    error = strdup("");
    fclose (file);
cleanup:
    zstr_free(&uri);
    zstr_free(&filename);
    zstr_free(&timeout_str);
    zstr_free(&uid);
    zstr_free(&peerid);
    zstr_free(&success);
    zstr_free(&error);

    zpoller_destroy(&poller);
    zsock_destroy(&router);
}

///////////////////////////////////////////////////
// helper functions

json_t * load_config_file(char* file) {
	/**
	 * loads the config file
	 *
	 * @param char* giving the path and filename to be loaded. Must point to a JSON file.
	 *
	 * @return jansson encoded json_t* (new reference) containing the content of the file as json object
	 */
    json_error_t error;
    json_t * root;
    root = json_load_file(file, JSON_ENSURE_ASCII, &error);
    if(!root) {
    	printf("Error parsing JSON file! file: %s, line %d: %s\n", error.source, error.line, error.text);
    	return NULL;
    }
    printf("[%s] config file: %s\n", json_string_value(json_object_get(root, "short-name")), json_dumps(root, JSON_ENCODE_ANY));
    return root;
}

char* encode_msg(char* metamodel, char* model, const char* type, json_t* payload) {
	/**
	 * encodes a Sherpa msg in its proper form
	 *
	 * @param char* to metamodel
	 * @param char* to model
	 * @param char* to msg type
	 * @param jansson encoded json_t* to payload
	 *
	 * @return returns a char* that can be sent by zyre (user must free it afterwards)
	 */
	json_t *msg;
	msg = json_object();
	json_object_set(msg, "metamodel", json_string(metamodel));
	json_object_set(msg, "model", json_string(model));
	json_object_set(msg, "type", json_string(type));
	json_object_set(msg, "payload", payload);
	char *ret = json_dumps(msg, JSON_ENCODE_ANY);
	json_decref(msg);
	return ret;
}

int decode_json(char* message, json_msg_t *result) {
	/**
	 * decodes a received msg to json_msg types
	 *
	 * @param received msg as char*
	 * @param json_msg_t* at which the result is stored
	 *
	 * @return returns 0 if successful and -1 if an error occurred
	 */
    json_t *root;
    json_error_t error;
    root = json_loads(message, 0, &error);

    if(!root) {
    	printf("Error parsing JSON string! line %d: %s\n", error.line, error.text);
    	return -1;
    }
    if (json_object_get(root, "metamodel")) {
    	result->metamodel = strdup(json_string_value(json_object_get(root, "metamodel")));
    } else {
    	printf("Error parsing JSON string! Does not conform to msg model. No metamodel specified.\n");
    	return -1;
    }
    if (json_object_get(root, "model")) {
		result->model = strdup(json_string_value(json_object_get(root, "model")));
	} else {
		printf("Error parsing JSON string! Does not conform to msg model. No model specified.\n");
		return -1;
	}
    if (json_object_get(root, "type")) {
		result->type = strdup(json_string_value(json_object_get(root, "type")));
	} else {
		printf("Error parsing JSON string! Does not conform to msg model.\n");
		return -1;
	}
    if (json_object_get(root, "payload")) {
    	result->payload = strdup(json_dumps(json_object_get(root, "payload"), JSON_ENCODE_ANY));
	} else {
		printf("Error parsing JSON string! Does not conform to msg model. No payload specified.\n");
		return -1;
	}
    json_decref(root);
    return 0;
}
#endif
