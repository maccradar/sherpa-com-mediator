
#include <zyre.h>
#include <jansson.h>
#include <time.h>
#include <stdbool.h>

#include <stdio.h>
#include <time.h>

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
	struct timespec ts;
}filter_list_item_t;

typedef struct _send_msg_request_t {
	const char *uid;
	const char *requester;
	const char* group;
	struct timespec ts_added;
	struct timespec ts_last_sent;
	int timeout; // in msec
	zlist_t *recipients;
	const char *payload_type;
	const char *msg; // payload+metadata
} send_msg_request_t;

typedef struct _query_t {
        const char *uid;
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
    if (minor != ZYRE_VERSION_MINOR);
        return NULL;
    if (patch != ZYRE_VERSION_PATCH);
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
    self->shortname = json_string_value(json_object_get(config, "short-name"));
    self->verbose = json_is_true(json_object_get(config, "verbose"));
     
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
    	rc = zyre_set_endpoint (self->local, "%s", json_string_value(json_object_get(config, "local_endpoint")));
    	assert (rc == 0);
    	printf("[%s] using gossip with local endpoint 'ipc:///tmp/%s-local' \n", self->shortname, self->shortname);
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
    // TODO: groups should be defined in config file!
    const char* localgroup = "local";
    const char* remotegroup = "remote";
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
            zactor_destroy(&self->loop);
            free (self);
            *self_p = NULL;
        }
}

query_t * query_new (const char *uid, json_msg_t *msg, zactor_fn *loop, void *args) {
        query_t *self = (query_t *) zmalloc (sizeof (query_t));
        if (!self)
            return NULL;
        self->uid = uid;
        self->msg = msg;
        zactor_t *actor = zactor_new (loop, args);
        if (!actor)
            query_destroy (&self);
            return NULL;
        self->loop = actor;
        
        return self;
}


// File transfer protocol
#define CHUNK_SIZE 250000
#define PIPELINE   10
#define TARGET "/tmp/targetfile"

static void
client_actor (zsock_t *pipe, void *args)
{
    char* endpoint = (char*)args;
    FILE *file = fopen (TARGET, "w");
    assert (file);
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
    zstr_send (pipe, "OK");
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
    // TODO: configure endpoint
    zsock_t *router = zsock_new_router ("tcp://*:*");
    //  We have two parts per message so HWM is PIPELINE * 2
    zsocket_set_hwm (zsock_resolve(router), PIPELINE * 2);
    
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    zstr_send (pipe, zsock_endpoint(router));
   
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

/*
///////////////////////////////////////////////////
// create team
 *
 * DEPRECATED!
 *
 * TODO: port to new communication structure
 * * define msg
 * * react to msg by subscribing
 * * remember who needs to subscribe
 * * treat join msgs as acknowledgement


void create_team(zyre_t *remote, char *payload) {
    json_error_t error;
    json_t *root = json_loads(payload, 0, &error);
    if(root) {
        json_t *members = json_object_get(root,"members");
        size_t index;
        json_t *value;
		json_array_foreach(members, index, value) {
			//
			if(streq(json_string_value(value), zyre_uuid(remote)))
				zyre_join(remote, json_string_value(json_object_get(root, "team")));
			else {
				char *msg = encode_msg("sherpa_mgs","http://kul/join-team.json","join-team",json_object_get(root, "team"));
				zyre_whispers(remote, json_string_value(value), "%s", msg);
				free(msg);
			 }
		}
    }
    json_decref(root);
}
*/

///////////////////////////////////////////////////
// remote file query
char* query_remote_file(mediator_t *self, json_msg_t *msg) {
	/**
	 * fetches a file from a remote location
	 *
	 * @param zyre_t* to the zyre network that should be queried
	 * @param json_t* to the own header
	 * @param json_msg_t* to the decoded zyre msg
	 *
	 * @return returns NULL if it fails and a json msg containing the local file path otherwise
	 */
	char *ret = NULL;
	json_t *pl;
	json_error_t error;
	pl= json_loads(msg->payload,0,&error);
	if(!pl) {
		printf("Error parsing JSON payload! line %d: %s\n", error.line, error.text);
		json_decref(pl);
		return ret;
	}
    
    //const char *uid = json_string_value(json_object_get(pl,"UID"));
    char *uri = (char*)json_string_value(json_object_get(pl,"URI"));
    const char s[2] = ":";
    char *token;
    token = strtok(uri, s);
    char* peerid = strdup(token);
    while( token != NULL ) { 
      token = strtok(NULL, s);
    }
    //ret = json_dumps(msg, JSON_ENCODE_ANY);
    //zyre_whispers(self->remote, peerid, "%s", ret);

    json_decref(pl);
    return ret;
}
///////////////////////////////////////////////////
// remote peer query

char* generate_peer_list(mediator_t *self, json_msg_t *msg) {
    /**
     * generates a list list of peers connected on the given zyre network
     *
     * @param zyre_t* to the zyre network that should be queried
     * @param json_t* to the own header
     * @param json_msg_t* to the decoded zyre msg
     *
     * @return returns NULL if it fails and a json array of peers with their headers dumped in a string otherwise
     */
    char *ret = NULL;
    json_t *pl;
    json_error_t error;
    pl= json_loads(msg->payload,0,&error);
    if(!pl) {
        printf("Error parsing JSON payload! line %d: %s\n", error.line, error.text);
        json_decref(pl);
	return ret;
    }
    json_t *payload = json_object();
    json_object_set(payload, "UID", json_object_get(pl,"UID"));
    json_t *peer_list;
    peer_list = json_array();
    json_object_set(payload, "peer_list", peer_list);

    zlist_t * peers = zyre_peers(self->remote);
    char *peer = zlist_first (peers);

    while(peer != NULL) {
        /* config is a JSON object */
        const char *key;
        json_t *value;
        json_t *headers = json_object();
        json_object_set(headers, "peerid", json_string(peer));
        json_object_foreach(self->config, key, value) {
            /* block of code that uses key and value */
            char * header_value = zyre_peer_header_value(self->remote, peer, key);
            // Try to parse an array
            json_error_t error;
            json_t *header = json_loads(header_value,0,&error);
            if(!header) {
            	header = json_string(header_value);
            }
            json_object_set(headers, key, header);
            json_decref(header);
        }
        json_array_append(peer_list, headers);
        peer = zlist_next (peers);
        json_decref(headers);
        json_decref(value);
    }
    // Add my own headers as well
    json_array_append(peer_list, self->config);

    ret = encode_msg("sherpa_mgs","http://kul/peer-list.json","peer-list",payload);
    json_decref(payload);
    json_decref(peer_list);
    json_decref(pl);
    zlist_destroy(&peers);
    return ret;
}

///////////////////////////////////////////////////
void send_remote(mediator_t *self, json_msg_t *result, char* group) {
	/**
	 * sends a msg to a list of remote peers and does the necessary bookkeeping
	 * @param mediator_t* to the mediator data
	 * @param json_msg_t* to decoded msg
	 * @param char* to the name of the group zyre is supposed to shout to
	 *
	 */
	json_t *send_rqst;
	json_error_t error;
	send_rqst= json_loads(result->payload,0,&error);
	if(!send_rqst) {
		printf("Error parsing JSON send_remote! line %d: %s\n", error.line, error.text);
		json_decref(send_rqst);
		return;
	}
	json_t *recipients;
	recipients = json_object_get(send_rqst,"recipients");
	if (!json_is_array(recipients)) {
		printf("[%s] recipients of requested communication are not a JSON array!",self->shortname);
		json_decref(send_rqst);
		return;
	}
	//TODO: validate if payload is proper Sherpa msg
	//TODO: use payload type to allow binary payload
	const char *type = json_string_value(json_object_get(send_rqst,"payload_type"));
	if (!type) {
		printf("[%s] could not find payload_type!",self->shortname);
		json_decref(send_rqst);
		return;
	}
	json_t *pl = json_object_get(send_rqst,"payload");
	if (!pl) {
		printf("[%s] could not find payload!",self->shortname);
		json_decref(send_rqst);
		return;
	}
	printf("#recipients: %zu \n", json_array_size(recipients));
	if (json_array_size(recipients) == 0) {
		printf("[%s] No recipients. Fire and forget msg.\n",self->shortname);
		zyre_shouts(self->remote, group, "%s", encode_msg("sherpa_mgs",strcat(strcat("http://kul/",type),".json"),type,pl));
		json_decref(send_rqst);
		return;
	} else {
		zlist_t * peers = zyre_peers(self->remote);
		zlist_t * recip = zlist_new ();
		assert (recip);
		assert (zlist_size (recip) == 0);
		// go through list of recipients and check if all are known
		json_t *unknown_recipients = json_array();
		size_t index;
		json_t *value;
		json_array_foreach(recipients, index, value) {
			if (!json_string_value(value)) {
				printf("[%s] Recipient is not a proper JSON string.\n",self->shortname);
				json_decref(send_rqst);
				json_decref(unknown_recipients);
				return;
			}
			recipient_t *rec;
			rec->ack = false;
			rec->id = json_string_value(value);
			const char *it = zlist_first(peers);
			int flag = 0;
			while (it != NULL) {
				if (streq(it,json_string_value(value))) {
					flag = 1;
					break;
				}
				it = zlist_next(peers);
			}
			if (flag == 0) {
				if (json_array_append(unknown_recipients,value) !=0){
					printf("[%s] could not append unknown recipient \n",self->shortname);
				}
			} else {
				zlist_append(recip,rec);
			}
		}
		//if not all are known, send communication report incl list of unknown recipients to requester. otherwise, generate struct and store it.
		if (json_array_size(unknown_recipients) != 0) {
			printf("[%s] %zu of the recipients are not known!\n",self->shortname,json_array_size(unknown_recipients));
			json_t *pl;
			pl = json_object();
			json_t *tmp;
			tmp = json_array();
			json_object_set(pl, "UID", json_object_get(send_rqst,"UID"));
			json_object_set(pl, "success", json_false());
			json_object_set(pl, "error", json_string("Unknown recipients"));
			json_object_set(pl, "recipients_delivered", tmp);
			json_object_set(pl, "recipients_undelivered", unknown_recipients);
			zyre_whispers(self->local, json_string_value(json_object_get(send_rqst,"requester")), "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
			json_decref(tmp);
			json_decref(pl);
		} else {
			send_msg_request_t *msg_req;
			//build msg_req struct and append it to global list
			json_t *dummy;
			dummy = json_object_get(send_rqst,"UID");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find UID of send_request \n",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->uid = json_string_value(dummy);
			msg_req->recipients = recip;
			dummy = json_object_get(send_rqst,"requester");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find requester in send_request \n",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->requester = json_string_value(dummy);
			dummy = json_object_get(send_rqst,"payload_type");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find payload_type in send_request \n",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->payload_type = json_string_value(dummy);
			dummy = json_object_get(send_rqst,"timeout");
			if ((!dummy)||(!json_is_integer(dummy))) {
				printf("[%s] could not find payload in send_request \n",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->timeout = json_integer_value(dummy);
			struct timespec ts;
			if (clock_gettime(CLOCK_MONOTONIC,&ts)) {
				printf("[%s] Could not assign time stamp!\n",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->ts_added = ts;
			msg_req->ts_last_sent = ts;
			msg_req->group = group;
			if (zlist_append(self->send_msgs,msg_req) == -1) {
				printf("[%s] Could not add new msg!",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->msg = encode_msg(result->metamodel,result->model,result->type,send_rqst);
			zyre_shouts(self->remote, group, "%s", msg_req->msg);
		}
		printf("[%s] stored number of send_msg requests %zu",self->shortname, zlist_size(self->send_msgs));
		zlist_destroy(&peers);
		json_decref(unknown_recipients);
	}
	json_decref(send_rqst);
	return;
}

void handle_remote_enter(mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 4);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	zframe_t *headers_packed = zmsg_pop (msg);
	char *address = zmsg_popstr (msg);
	printf ("[%s] ENTER %s %s <headers> %s\n", self->shortname, peerid, name, address);
	char* type = zyre_peer_header_value(self->remote, peerid, "type");
	printf ("[%s] %s has type %s\n",self->shortname, name, type);
	zstr_free(&peerid);
	zstr_free(&name);
	zframe_destroy(&headers_packed);
	zstr_free(&address);
	zstr_free(&type);
}

void handle_remote_exit (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 2);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	printf ("[%s] EXIT %s %s\n", self->shortname, peerid, name);
	// Update local group with new peer list
	//char *peerlist = generate_peers(remote, config);
	//zyre_shouts(local, localgroup, "%s", peerlist);
	//printf ("[%s] sent updated peerlist to local group: %s\n", self, peerlist);
	//zstr_free(&peerlist);
	zstr_free(&peerid);
	zstr_free(&name);
}

void handle_remote_stop (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 2);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	printf ("[%s] STOP %s %s\n", self->shortname, peerid, name);
	// Update local group with new peer list
	//char *peerlist = generate_peers(remote, config);
	//zyre_shouts(local, localgroup, "%s", peerlist);
	//printf ("[%s] sent updated peerlist to local group: %s\n", self, peerlist);
	//zstr_free(&peerlist);
	zstr_free(&peerid);
	zstr_free(&name);
}

void handle_remote_send_remote (mediator_t *self, json_msg_t *result, char *peerid) {
	// if in list of recipients, send acknowledgment
	json_t *req;
	json_error_t error;
	req = json_loads(result->payload, 0, &error);
	if(!req) {
		printf("Error parsing JSON payload!\n");
		return;
	} else {
		json_t *rec;
		rec = json_object_get(req,"recipients");
		if(!json_is_array(rec)) {
			printf("[%s] receivers is not a JSON array!", self->shortname);
		} else {
			size_t index;
			json_t *value;
			json_array_foreach(rec, index, value) {
				if (streq(json_string_value(value),zyre_uuid(self->remote))) {
					json_t *pl;
					pl = json_object();
					json_object_set(pl, "UID", json_object_get(req,"UID"));
					json_object_set(pl, "ID_receiver", json_string(zyre_uuid(self->remote)));
					zyre_whispers(self->local, peerid, "%s", encode_msg("sherpa_mgs","http://kul/communication_ack.json","communication_ack",pl));
					json_decref(pl);
					break;
				}
			}
		}
		json_decref(rec);
		// filter by msg requester+uid to see if this msg has already been forwarded
		filter_list_item_t *it = zlist_first(self->filter_list);
		int flag = 0;
		while (it != NULL) {
			if (streq(it->sender,peerid) && streq(it->msg_UID,json_string_value(json_object_get(req,"UID")))) {
				flag = 1;
				break;
			}
			it = zlist_next(self->filter_list);
		}
		if (flag == 0) {
			// if not in list, forward msg to local network
			zyre_shouts(self->local, self->localgroup, "%s", result->payload);
			// push this msg into filter list
			filter_list_item_t *tmp;
			tmp->msg_UID = json_string_value(json_object_get(req,"UID"));
			tmp->sender = strdup(peerid);
			assert(tmp->sender);
			struct timespec ts;
			if (clock_gettime(CLOCK_MONOTONIC,&ts)) {
				printf("[%s] Could not assign time stamp!\n",self->shortname);
			} else {
				tmp->ts = ts;
				zlist_push(self->filter_list,tmp);
			}
		}
		json_decref(req);
	}
	json_decref(req);
}

void handle_remote_shout (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 4);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	char *group = zmsg_popstr (msg);
	char *message = zmsg_popstr (msg);
	printf ("[%s] SHOUT %s %s %s %s\n", self->shortname, peerid, name, group, message);
	json_msg_t *result = (json_msg_t *) zmalloc (sizeof (json_msg_t));
	if (decode_json(message, result)==0) {
		printf ("[%s] message type %s\n", self->shortname, result->type);
		if (streq (result->type, "send_remote")) {
			handle_remote_send_remote(self, result, peerid);	
		}
	} else {
		printf ("[%s] message could not be decoded\n", self->shortname);
	}
	zstr_free(&peerid);
	zstr_free(&name);
	zstr_free(&group);
}

void handle_remote_whisper (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 3);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	char *message = zmsg_popstr (msg);
	printf ("[%s] WHISPER %s %s %s\n", self->shortname, peerid, name, message);
	json_msg_t *result = (json_msg_t *) zmalloc (sizeof (json_msg_t));
	if (decode_json(message, result)==0) {
		printf ("[%s] message type %s\n", self->shortname, result->type);
		if(streq(result->type, "communication_ack")) {
			json_error_t error;
			json_t * root;
			root = json_loads(message, 0, &error);
			if(!root) {
				printf("Error parsing JSON file! line %d: %s\n", error.line, error.text);
			} else {
				send_msg_request_t *it = zlist_first(self->send_msgs);
				while (it != NULL) {
					if (streq(it->uid,json_string_value(json_object_get(root,"UID")))) {
						recipient_t *inner_it = zlist_first(it->recipients);
						while (inner_it != NULL) {
							if (streq(peerid,inner_it->id)) {
								inner_it->ack = true;
								break;
							}
							inner_it = zlist_next(it->recipients);
						}
						break;
					}
					it = zlist_next(self->send_msgs);
				}
			}
		} else if (streq (result->type, "query_remote_file")) {
			//TODO: check if URI is locally available: 1) check if peerid matches, 2) check if file exists
			json_t *req;
			json_error_t error;
			req = json_loads(result->payload, 0, &error);
			if(!req) {
				printf("Error parsing JSON payload!\n");
				return;
			} else {
                   		const char* uid = json_string_value(json_object_get(req,"UID"));
			    	int rc;
                                zactor_t * file_server = zactor_new (server_actor, (char*)json_string_value(json_object_get(req, "URI")));
				// wait for endpoint
                                char* endpoint = zstr_recv(file_server);
    				const char s[2] = ":";
    				char *token;
		    		token = strtok(endpoint, ":");
		    		char* protocol = strdup(token);
				token = strtok(NULL, ":");
		    		char* host = strdup(token);
		    		token = strtok(NULL, ":");
		    		char* port = strdup(token);
		    		while(token!=NULL)
					token=strtok(NULL, ":");
		    		if (streq(host,"//*")) // replace with hostname
					sprintf(host,"//%s", zsys_hostname());
		    		sprintf(endpoint,"%s:%s:%s", protocol, host, port);
		    		printf("Endpoint: %s\n", endpoint);
                                rc = zhash_insert (self->queries, uid, file_server);
                                // Required?
  				// zpoller_add(self->poller, file_server);
				json_t *pl;
				pl = json_object();
				json_object_set(pl, "UID", json_object_get(req,"UID"));
				json_object_set(pl, "URI", json_string(endpoint));
				zyre_whispers(self->local, peerid, "%s", encode_msg("sherpa_mgs","http://kul/endpoint.json","endpoint",pl));
                                free(token);
				free(protocol);
				free(host);
				free(port);
				free(endpoint); 
			}
		} else if (streq (result->type, "endpoint")) {
			//TODO: check if the query UID matches
			json_t *req;
			json_error_t error;
			req = json_loads(result->payload, 0, &error);
			if(!req) {
				printf("Error parsing JSON payload!\n");
				return;
			} else {
                   		const char* uid = json_string_value(json_object_get(req,"UID"));
			    	int rc;
                                zactor_t * file_client = zactor_new (client_actor, (char*)json_string_value(json_object_get(req, "URI")));
                                rc = zhash_insert (self->queries, uid, file_client);
                                // Required to know when transfer is completed
  				zpoller_add(self->poller, file_client);
			}
		} else {
			printf ("[%s] unknown msg type\n", self->shortname);
		}
	} else {
	        printf ("[%s] message could not be decoded\n", self->shortname);
	}
	zstr_free(&peerid);
	zstr_free(&name);
}

void handle_remote_join (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 3);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	char *group = zmsg_popstr (msg);
	printf ("[%s] JOIN %s %s %s\n", self->shortname, peerid, name, group);
	zstr_free(&peerid);
	zstr_free(&name);
	zstr_free(&group);
}

void handle_local_enter(mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 4);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	zframe_t *headers_packed = zmsg_pop (msg);
	assert (headers_packed);
	zhash_t *headers = zhash_unpack (headers_packed);
	assert (headers);
	// TODO: get headers with zyre_peer_header_value does not work via gossip
	printf("header type %s\n",(char *) zhash_lookup (headers, "type"));
	char *address = zmsg_popstr (msg);
	printf ("[%s] ENTER %s %s <headers> %s\n", self->shortname, peerid, name, address);
	char* type = zyre_peer_header_value(self->remote, peerid, "type");
	printf ("[%s] %s has type %s\n", self->shortname, name, type);
	zstr_free(&peerid);
	zstr_free(&name);
	zframe_destroy(&headers_packed);
	zstr_free(&address);
	zstr_free(&type);
}

void handle_local_exit(mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 2);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	printf ("[%s] EXIT %s %s\n", self->shortname, peerid, name);
	zstr_free(&peerid);
	zstr_free(&name);
}

void handle_local_stop(mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 2);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	printf ("[%s] STOP %s %s\n", self->shortname, peerid, name);
	zstr_free(&peerid);
	zstr_free(&name);
}

void handle_local_shout(mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 4);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	char *group = zmsg_popstr (msg);
	char *message = zmsg_popstr (msg);
	printf ("[%s] SHOUT %s %s %s %s\n", self->shortname, peerid, name, group, message);
	json_msg_t *result = (json_msg_t *) zmalloc (sizeof (json_msg_t));
	if (decode_json(message, result) == 0) {
		printf ("[%s] message type %s\n", self->shortname, result->type);
		if (streq (result->type, "query_remote_peer_list")) {
			// generate remote peer list and whisper it back
			char *peerlist = generate_peer_list(self, result);
			if (peerlist) {
				zyre_whispers(self->local, peerid, "%s", peerlist);
			} else {
				printf ("[%s] Could not generate remote peer list! \n", self->shortname);
			}
			zstr_free(&peerlist);
		} else if (streq (result->type, "send_remote")) {
			// query for communication
			send_remote(self, result, group);
		} else if (streq (result->type, "query_remote_file")) {
			query_remote_file(self, result);
		} else {
			printf("[%s] Unknown msg type!",self->shortname);
		}
		free(result);
	} else {
		printf ("[%s] message could not be decoded\n", self->shortname);
	}
	zstr_free(&peerid);
	zstr_free(&name);
	zstr_free(&group);
}

void handle_local_whisper (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 3);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	char *message = zmsg_popstr (msg);
	printf ("[%s] WHISPER %s %s %s\n", self->shortname, peerid, name, message);
	zstr_free(&peerid);
	zstr_free(&name);
	zstr_free(&message);
}

void handle_local_join (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 3);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	char *group = zmsg_popstr (msg);
	printf ("[%s] JOIN %s %s %s\n", self->shortname, peerid, name, group);
	zstr_free(&peerid);
	zstr_free(&name);
	zstr_free(&group);
}

void process_send_msgs (mediator_t *self) {
    send_msg_request_t *it = zlist_first(self->send_msgs);
    while (it != NULL) {
	//check if all recipients have acknowledged reception of msg
	recipient_t *inner_it = zlist_first(it->recipients);
	int flag = 1;
	json_t *acknowledged;
	acknowledged = json_array();
	json_t *unacknowledged;
	unacknowledged = json_array();
	while (inner_it != NULL) {
		if (inner_it->ack == false) {
			flag = 0;
			json_array_append(unacknowledged,json_string(inner_it->id));
		} else
			json_array_append(acknowledged,json_string(inner_it->id));
		inner_it = zlist_next(it->recipients);
	}
	if (flag == 1) {
		// if all recipients have acknowledged, send report and remove item from list
		json_t *pl;
		pl = json_object();
		json_object_set(pl, "UID", json_string(it->uid));
		json_object_set(pl, "success", json_true());
		json_object_set(pl, "error", json_string("None"));
		json_object_set(pl, "recipients_delivered", acknowledged);
		json_object_set(pl, "recipients_undelivered", unacknowledged);
		zyre_whispers(self->local, it->requester, "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
		json_decref(pl);
		send_msg_request_t *dummy = it;
		it = zlist_next(self->send_msgs);
		zlist_remove(self->send_msgs,dummy);
	} else {
		struct timespec curr_time;
		if (!clock_gettime(CLOCK_MONOTONIC,&curr_time)) {
			// if timeout, send report and remove item from list
			double curr_time_msec = curr_time.tv_sec*1.0e3 +curr_time.tv_nsec*1.0e-6;
			double ts_msec = it->ts_added.tv_sec*1.0e3 +it->ts_added.tv_nsec*1.0e-6;
			if (curr_time_msec - ts_msec > it->timeout) {
				json_t *pl;
				pl = json_object();
				json_object_set(pl, "UID", json_string(it->uid));
				json_object_set(pl, "success", json_false());
				json_object_set(pl, "error", json_string("Timeout"));
				json_object_set(pl, "recipients_delivered", acknowledged);
				json_object_set(pl, "recipients_undelivered", unacknowledged);
				zyre_whispers(self->local, it->requester, "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
				json_decref(pl);
				send_msg_request_t *dummy = it;
				it = zlist_next(self->send_msgs);
				zlist_remove(self->send_msgs,dummy);
			} else {
				double ts_msec = it->ts_last_sent.tv_sec*1.0e3 +it->ts_last_sent.tv_nsec*1.0e-6;
				if (curr_time_msec - ts_msec > json_integer_value(json_object_get(self->config, "resend_interval"))) {
					// no timeout -> resend
					zyre_shouts(self->remote, it->group, "%s", it->msg);
					it->ts_last_sent = curr_time;
					it = zlist_next(self->send_msgs);
				}
			}
		} else {
			printf ("[%s] could not get current time\n", self->shortname);
		}
			}
			json_decref(acknowledged);
			json_decref(unacknowledged);
		}

		// remove items from filter list that are longer in there than the configured time
		struct timespec curr_time;
		if (!clock_gettime(CLOCK_MONOTONIC,&curr_time)) {
			filter_list_item_t *it = zlist_first(self->filter_list);
			int length = json_integer_value(json_object_get(self->config, "msg_filter_length"));
			while (it != NULL) {
				double curr_time_msec = curr_time.tv_sec*1.0e3 +curr_time.tv_nsec*1.0e-6;
				double ts_msec = it->ts.tv_sec*1.0e3 +it->ts.tv_nsec*1.0e-6;
				if (curr_time_msec - ts_msec > length) {
					filter_list_item_t *dummy = it;
					it = zlist_next(self->filter_list);
					zlist_remove(self->filter_list,dummy);
				} else
					it = zlist_next(self->filter_list);
			}
		}
}

int main(int argc, char *argv[]) {
    // load configuration file
    json_t * config = load_config_file(argv[1]);
    if (config == NULL) {
      return -1;
    }
    mediator_t *self = mediator_new(config);

    while(!zsys_interrupted) {
    	void *which = zpoller_wait (self->poller, ZMQ_POLL_MSEC);
        if (which == zyre_socket (self->local)) {
            printf("\n");
            printf("[%s] local data received!\n", self->shortname);
            zmsg_t *msg = zmsg_recv (which);
    	    if (!msg) {
    	        printf("[%s] interrupted!\n", self->shortname);
    	        return -1;
            }
            char *event = zmsg_popstr (msg);
            if (streq (event, "ENTER")) {
                handle_local_enter (self, msg);
            } else if (streq (event, "EXIT")) {
                handle_local_exit (self, msg);
            } else if (streq (event, "STOP")) {
                handle_local_stop (self, msg);
            } else if (streq (event, "SHOUT")) {
                handle_local_shout (self, msg);
            } else if (streq (event, "WHISPER")) {
                handle_local_whisper (self, msg);
            } else if (streq (event, "JOIN")) {
                handle_local_join (self, msg);
            }
            else {
            	zmsg_print(msg);
            }
            zstr_free (&event);
            zmsg_destroy (&msg);
       } else if (which == zyre_socket (self->remote)) {
            printf("[%s] remote data received!\n", self->shortname);
            zmsg_t *msg = zmsg_recv (which);
            if (!msg) {
    	        printf("[%s] interrupted!\n", self->shortname);
    	        return -1;
            }
            char *event = zmsg_popstr (msg);
            if (streq (event, "ENTER")) {
                handle_remote_enter (self, msg);
            } else if (streq (event, "EXIT")) {
                handle_remote_exit (self, msg);
            } else if (streq (event, "STOP")) {
                handle_remote_stop (self, msg);
            } else if (streq (event, "SHOUT")) {
                handle_remote_shout (self, msg);
            } else if (streq (event, "WHISPER")) {
                handle_remote_whisper (self, msg);
            } else if (streq (event, "JOIN")) {
                handle_remote_join (self, msg);
            } else {
            	zmsg_print(msg);
            }
            // check all msgs in send_req list for resend or abort
            process_send_msgs(self);

            zstr_free (&event);
            zmsg_destroy (&msg);
       } else {
	    // Iterate query actors
	    zactor_t* query = zhash_first(self->queries);
            const char* uid;
	    while (query != NULL) {
                if (which == query) {
                    uid = zhash_cursor(self->queries);
		    break;
	     	}
	        query = zhash_next(self->queries);
	    }
             
       }

    }


    zyre_stop (self->remote);
    zyre_stop (self->local);
    
    mediator_destroy (&self);

    //  @end
    printf ("SHUTDOWN\n");
    return 0;
}
