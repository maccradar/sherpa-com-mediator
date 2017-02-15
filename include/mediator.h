#ifndef MEDIATOR_H
#define MEDIATOR_H

#include <zyre.h>
#include <jansson.h>

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
    char* peerid = ((char**)args)[0];
    char* uid = ((char**)args)[1];
    char* endpoint = ((char**)args)[2];
    char* target = ((char**)args)[3];
    FILE *file = fopen (target, "w");
    //assert (file);
    if(!file) {
    	printf ("Cannot open target file %s for file transfer: \n", target);
    	return;
    }
    zsock_t *dealer = zsock_new_dealer(endpoint);
    
    //  Up to this many chunks in transit
    size_t credit = PIPELINE;
    
    size_t total = 0;       //  Total bytes received
    size_t chunks = 0;      //  Total chunks received
    size_t offset = 0;      //  Offset of next chunk request
    
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    
    bool terminated = false;
    zpoller_t *poller = zpoller_new (pipe, NULL);
    while (!terminated) {
        void *which = zpoller_wait (poller, 1);
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (which);
            if (!msg)
                break;              //  Interrupted
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM"))
                terminated = true;
        }
        while (credit) {
            //  Ask for next chunk
            zstr_sendm  (dealer, "fetch");
            zstr_sendfm (dealer, "%ld", offset);
            zstr_sendf  (dealer, "%ld", (long) CHUNK_SIZE);
            offset += CHUNK_SIZE;
            credit--;
        }
        zframe_t *chunk = zframe_recv (dealer);
        if (!chunk)
            break;              //  Shutting down, quit
        chunks++;
        credit++;
        size_t size = zframe_size (chunk);
        fwrite (zframe_data(chunk) , sizeof(char), size, file);
        zframe_destroy (&chunk);
        total += size;
        if (size < CHUNK_SIZE)
            break;//terminated = true;              //  Last chunk received; exit
    } 
    printf ("%zd chunks received, %zd bytes\n", chunks, total);
    fclose(file);
    // Query type
    zstr_sendm (pipe, "remote_file_done");
    zstr_sendm (pipe, peerid);
    zstr_sendm (pipe, uid);
    zstr_send (pipe, target);
    
    zpoller_destroy(&poller);
    zsock_destroy(&dealer);
}

//  The server thread waits for a chunk request from a client,
//  reads that chunk and sends it back to the client:

static void
server_actor (zsock_t *pipe, void *args)
{
    // TODO: use JSON with parsed URI upfront?
    char* uri = strdup((char*)args);
    char* token;
    token = strtok(uri, ":"); // Remove host/peerid
    token = strtok(NULL, ":");
    char* filename = strdup(token);
    FILE *file = fopen (filename, "r");
    assert (file);
    zsock_t *router = zsock_new_router ("tcp://*:*");

    //  We have two parts per message so HWM is PIPELINE * 2
    zsocket_set_hwm (zsock_resolve(router), PIPELINE * 2);
    
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    // Inform caller our endpoint
    zstr_send (pipe, zsock_endpoint(router));
   
    bool terminated = false;
    zpoller_t *poller = zpoller_new (pipe, router);
    while (!terminated) {
        void *which = zpoller_wait (poller, 1);
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (which);
            if (!msg)
                break;              //  Interrupted

            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM"))
                terminated = true;
        } else if (which == router) {    
            //  First frame in each message is the sender identity
            zframe_t *identity = zframe_recv (router);
            if (!identity)
                break;              //  Shutting down, quit
            
            //  Second frame is "fetch" command
            char *command = zstr_recv (router);
            assert (streq (command, "fetch"));
            free (command);

            //  Third frame is chunk offset in file
            char *offset_str = zstr_recv (router);
            size_t offset = atoi (offset_str);
            free (offset_str);

            //  Fourth frame is maximum chunk size
            char *chunksz_str = zstr_recv (router);
            size_t chunksz = atoi (chunksz_str);
            free (chunksz_str);

            //  Read chunk of data from file
            fseek (file, offset, SEEK_SET);
            byte *data = malloc (chunksz);
            assert (data);

            //  Send resulting chunk to client
            size_t size = fread (data, 1, chunksz, file);
            zframe_t *chunk = zframe_new (data, size);
            zframe_send (&identity, router, ZFRAME_MORE);
            zframe_send (&chunk, router, 0);
        }
    }
    printf("Finished serving %s on %s\n", filename, zsock_endpoint(router));
    fclose (file);
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
    	printf("Error parsing JSON string! Does not conform to msg model.\n");
    	return -1;
    }
    if (json_object_get(root, "model")) {
		result->model = strdup(json_string_value(json_object_get(root, "model")));
	} else {
		printf("Error parsing JSON string! Does not conform to msg model.\n");
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
		printf("Error parsing JSON string! Does not conform to msg model.\n");
		return -1;
	}
    json_decref(root);
    return 0;
}
#endif
