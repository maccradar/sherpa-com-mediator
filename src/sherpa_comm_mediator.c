
#include <zyre.h>
#include <jansson.h>
#include <time.h>
#include <stdbool.h>

#include <stdio.h>
#include <time.h>

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
///////////////////////////////////////////////////
*/
// remote peer query

char* generate_peer_list(zyre_t *remote, json_t *config, json_msg_t *msg) {
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

    zlist_t * peers = zyre_peers(remote);
    char *peer = zlist_first (peers);

    while(peer != NULL) {
        /* config is a JSON object */
        const char *key;
        json_t *value;
        json_t *headers = json_object();
        json_object_set(headers, "peerid", json_string(peer));
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
            json_decref(header);
        }
        json_array_append(peer_list, headers);
        peer = zlist_next (peers);
        json_decref(headers);
        json_decref(value);
    }
    // Add my own headers as well
    json_array_append(peer_list, config);

    ret = encode_msg("sherpa_mgs","http://kul/peer-list.json","peer-list",payload);
    json_decref(payload);
    json_decref(peer_list);
    json_decref(pl);
    zlist_destroy(&peers);
    return ret;
}

///////////////////////////////////////////////////
void send_remote(json_msg_t *result, zlist_t *send_msgs, const char *self, char* group, zyre_t *remote, zyre_t *local) {
	/**
	 * sends a msg to a list of remote peers and does the necessary bookkeeping
	 *
	 * @param json_msg_t* to decoded msg
	 * @param zlist_t* to list of stored send_requests
	 * @param const char* to name of component
	 * @param char* to the name of the group zyre is supposed to shout to
	 * @param zyre_t* to remote network
	 * @param zyre_t* to local network
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
		printf("[%s] recipients of requested communication are not a JSON array!",self);
		json_decref(send_rqst);
		return;
	}
	//TODO: validate if payload is proper Sherpa msg
	//TODO: use payload type to allow binary payload
	const char *type = json_string_value(json_object_get(send_rqst,"payload_type"));
	if (!type) {
		printf("[%s] could not find payload_type!",self);
		json_decref(send_rqst);
		return;
	}
	json_t *pl = json_object_get(send_rqst,"payload");
	if (!pl) {
		printf("[%s] could not find payload!",self);
		json_decref(send_rqst);
		return;
	}
	printf("#recipients: %zu \n", json_array_size(recipients));
	if (json_array_size(recipients) == 0) {
		printf("[%s] No recipients. Fire and forget msg.\n",self);
		zyre_shouts(remote, group, "%s", encode_msg("sherpa_mgs",strcat(strcat("http://kul/",type),".json"),type,pl));
		json_decref(send_rqst);
		return;
	} else {
		zlist_t * peers = zyre_peers(remote);
		zlist_t * recip = zlist_new ();
		assert (recip);
		assert (zlist_size (recip) == 0);
		// go through list of recipients and check if all are known
		json_t *unknown_recipients = json_array();
		size_t index;
		json_t *value;
		json_array_foreach(recipients, index, value) {
			if (!json_string_value(value)) {
				printf("[%s] Recipient is not a proper JSON string.\n",self);
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
					printf("[%s] could not append unknown recipient \n",self);
				}
			} else {
				zlist_append(recip,rec);
			}
		}
		//if not all are known, send communication report incl list of unknown recipients to requester. otherwise, generate struct and store it.
		if (json_array_size(unknown_recipients) != 0) {
			printf("[%s] %zu of the recipients are not known!\n",self,json_array_size(unknown_recipients));
			json_t *pl;
			pl = json_object();
			json_t *tmp;
			tmp = json_array();
			json_object_set(pl, "UID", json_object_get(send_rqst,"UID"));
			json_object_set(pl, "success", json_false());
			json_object_set(pl, "error", json_string("Unknown recipients"));
			json_object_set(pl, "recipients_delivered", tmp);
			json_object_set(pl, "recipients_undelivered", unknown_recipients);
			zyre_whispers(local, json_string_value(json_object_get(send_rqst,"requester")), "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
			json_decref(tmp);
			json_decref(pl);
		} else {
			send_msg_request_t *msg_req;
			//build msg_req struct and append it to global list
			json_t *dummy;
			dummy = json_object_get(send_rqst,"UID");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find UID of send_request \n",self);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->uid = json_string_value(dummy);
			msg_req->recipients = recip;
			dummy = json_object_get(send_rqst,"requester");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find requester in send_request \n",self);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->requester = json_string_value(dummy);
			dummy = json_object_get(send_rqst,"payload_type");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find payload_type in send_request \n",self);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->payload_type = json_string_value(dummy);
			dummy = json_object_get(send_rqst,"timeout");
			if ((!dummy)||(!json_is_integer(dummy))) {
				printf("[%s] could not find payload in send_request \n",self);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->timeout = json_integer_value(dummy);
			struct timespec ts;
			if (clock_gettime(CLOCK_MONOTONIC,&ts)) {
				printf("[%s] Could not assign time stamp!\n",self);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->ts_added = ts;
			msg_req->ts_last_sent = ts;
			msg_req->group = group;
			if (zlist_append(send_msgs,msg_req) == -1) {
				printf("[%s] Could not add new msg!",self);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->msg = encode_msg(result->metamodel,result->model,result->type,send_rqst);
			zyre_shouts(remote, group, "%s", msg_req->msg);
		}
		printf("[%s] stored number of send_msg requests %zu",self, zlist_size(send_msgs));
		zlist_destroy(&peers);
		json_decref(unknown_recipients);
	}
	json_decref(send_rqst);
	return;
}

int main(int argc, char *argv[]) {
        
    int major, minor, patch;
    zyre_version (&major, &minor, &patch);
    assert (major == ZYRE_VERSION_MAJOR);
    assert (minor == ZYRE_VERSION_MINOR);
    assert (patch == ZYRE_VERSION_PATCH);
    
    //init list of send msg requests
    zlist_t *send_msgs = zlist_new();
    assert (send_msgs);
    assert (zlist_size (send_msgs) == 0);
    //init list for filtering msg requests
    zlist_t *filter_list = zlist_new();
    assert (filter_list);
    assert (zlist_size (filter_list) == 0);

    // load configuration file
    json_t * config = load_config_file(argv[1]);
    if (config == NULL) {
      return -1;
    }
    const char *self = json_string_value(json_object_get(config, "short-name"));
    bool verbose = json_is_true(json_object_get(config, "verbose"));
    
    //  Create two nodes: 
    //  - local gossip node for backend
    //  - remote udp node for frontend
    zyre_t *local = zyre_new (self);
    assert (local); 
    zyre_t *remote = zyre_new (self);
    assert (remote);
    printf("[%s] my remote UUID: %s\n", self, zyre_uuid(remote));
    json_object_set(config, "peerid", json_string(zyre_uuid(remote)));  
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
    	zyre_set_header(local, key, "%s", header_value);
        zyre_set_header(remote, key, "%s", header_value);
    }
 
    if(verbose)
    	zyre_set_verbose (local);
    
    int rc;
    if(!json_is_null(json_object_get(config, "gossip_endpoint"))) {
    	rc = zyre_set_endpoint (local, "%s", json_string_value(json_object_get(config, "local_endpoint")));
    	assert (rc == 0);
    	printf("[%s] using gossip with local endpoint 'ipc:///tmp/%s-local' \n", self, self);
    	//  Set up gossip network for this node
    	zyre_gossip_bind (local, "%s", json_string_value(json_object_get(config, "gossip_endpoint")));
    	printf("[%s] using gossip with gossip hub '%s' \n", self,json_string_value(json_object_get(config, "gossip_endpoint")));
    } else {
    	printf("[%s] WARNING: no local gossip communication is set! \n", self);
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
        	printf("\n");
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
                assert (headers_packed);
                zhash_t *headers = zhash_unpack (headers_packed);
                assert (headers);
                // TODO: get headers with zyre_peer_header_value does not work via gossip
                printf("test\n");
                printf("header type %s\n",(char *) zhash_lookup (headers, "type"));
                printf("test\n");
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
            	if (decode_json(message, result) == 0) {
            		printf ("[%s] message type %s\n", self, result->type);

            		if (streq (result->type, "query_remote_peer_list")) {
            			// generate remote peer list and whisper it back
            			char *peerlist = generate_peer_list(remote, config, result);
            			if (peerlist) {
            				zyre_whispers(local, peerid, "%s", peerlist);
            			} else {
            				printf ("[%s] Could not generate remote peer list! \n", self);
            			}
            			zstr_free(&peerlist);

					} else if (streq (result->type, "send_remote")) {
						// query for communication
						send_remote(result, send_msgs, self, remotegroup, remote, local);
					} else {
	            		printf("[%s] Unknown msg type!",self);
	            	}
            		free(result);
            	} else {
            		printf ("[%s] message could not be decoded\n", self);
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
                zstr_free(&peerid);
                zstr_free(&name);
                zstr_free(&message);
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
    	        // Update local group with new peer list
                //char *peerlist = generate_peers(remote, config);
                //zyre_shouts(local, localgroup, "%s", peerlist);
                //printf ("[%s] sent updated peerlist to local group: %s\n", self, peerlist);
                //zstr_free(&peerlist);
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
                if (decode_json(message, result)==0) {
					printf ("[%s] message type %s\n", self, result->type);
					if (streq (result->type, "send_remote")) {
						// if in list of recipients, send acknowledgment
						json_t *req;
						json_error_t error;
						req = json_loads(result->payload, 0, &error);
						if(!req) {
							printf("Error parsing JSON payload!\n");
							return -1;
						} else {
							json_t *rec;
							rec = json_object_get(req,"recipients");
							if(!json_is_array(rec)) {
								printf("[%s] receivers is not a JSON array!", self);
							} else {
								size_t index;
								json_t *value;
								json_array_foreach(rec, index, value) {
									if (streq(json_string_value(value),zyre_uuid(remote))) {
										json_t *pl;
										pl = json_object();
										json_object_set(pl, "UID", json_object_get(req,"UID"));
										json_object_set(pl, "ID_receiver", json_string(zyre_uuid(remote)));
										zyre_whispers(local, peerid, "%s", encode_msg("sherpa_mgs","http://kul/communication_ack.json","communication_ack",pl));
										json_decref(pl);
										break;
									}
								}
							}
							json_decref(rec);
							// filter by msg requester+uid to see if this msg has already been forwarded
							filter_list_item_t *it = zlist_first(filter_list);
							int flag = 0;
							while (it != NULL) {
								if (streq(it->sender,peerid) && streq(it->msg_UID,json_string_value(json_object_get(req,"UID")))) {
									flag = 1;
									break;
								}
								it = zlist_next(filter_list);
							}
							if (flag == 0) {
								// if not in list, forward msg to local network
								zyre_shouts(local, localgroup, "%s", result->payload);
								// push this msg into filter list
								filter_list_item_t *tmp;
								tmp->msg_UID = json_string_value(json_object_get(req,"UID"));
								tmp->sender = strdup(peerid);
								assert(tmp->sender);
								struct timespec ts;
								if (clock_gettime(CLOCK_MONOTONIC,&ts)) {
									printf("[%s] Could not assign time stamp!\n",self);
								} else {
									tmp->ts = ts;
									zlist_push(filter_list,tmp);
								}
							}
							json_decref(req);
						}
						json_decref(req);
					}
				} else {
					printf ("[%s] message could not be decoded\n", self);
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
                if (decode_json(message, result)==0) {
					printf ("[%s] message type %s\n", self, result->type);
					if(streq(result->type, "communication_ack")) {
						json_error_t error;
						json_t * root;
						root = json_loads(message, 0, &error);
						if(!root) {
							printf("Error parsing JSON file! line %d: %s\n", error.line, error.text);
						} else {
							send_msg_request_t *it = zlist_first(send_msgs);
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
								it = zlist_next(filter_list);
							}
						}
					} else {
						printf ("[%s] unknown msg type\n", self);
					}
				} else {
					printf ("[%s] message could not be decoded\n", self);
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
            // check all msgs in send_req list for resend or abort
            send_msg_request_t *it = zlist_first(send_msgs);
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
					zyre_whispers(local, it->requester, "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
					json_decref(pl);
					send_msg_request_t *dummy = it;
					it = zlist_next(send_msgs);
					zlist_remove(send_msgs,dummy);
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
							zyre_whispers(local, it->requester, "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
							json_decref(pl);
							send_msg_request_t *dummy = it;
							it = zlist_next(send_msgs);
							zlist_remove(send_msgs,dummy);
						} else {
							double ts_msec = it->ts_last_sent.tv_sec*1.0e3 +it->ts_last_sent.tv_nsec*1.0e-6;
							if (curr_time_msec - ts_msec > json_integer_value(json_object_get(config, "resend_interval"))) {
								// no timeout -> resend
								zyre_shouts(remote, it->group, "%s", it->msg);
								it->ts_last_sent = curr_time;
								it = zlist_next(filter_list);
							}
						}
					} else {
						printf ("[%s] could not get current time\n", self);
					}
				}
				json_decref(acknowledged);
				json_decref(unacknowledged);
			}

			// remove items from filter list that are longer in there than the configured time
			struct timespec curr_time;
			if (!clock_gettime(CLOCK_MONOTONIC,&curr_time)) {
				filter_list_item_t *it = zlist_first(filter_list);
				int length = json_integer_value(json_object_get(config, "msg_filter_length"));
				while (it != NULL) {
					double curr_time_msec = curr_time.tv_sec*1.0e3 +curr_time.tv_nsec*1.0e-6;
					double ts_msec = it->ts.tv_sec*1.0e3 +it->ts.tv_nsec*1.0e-6;
					if (curr_time_msec - ts_msec > length) {
						filter_list_item_t *dummy = it;
						it = zlist_next(filter_list);
						zlist_remove(filter_list,dummy);
					} else
						it = zlist_next(filter_list);
				}
			}

            zstr_free (&event);
            zmsg_destroy (&msg);
       }

    }

    // TODO: destroy the lists and their elements -> see bottom of http://czmq.zeromq.org/manual:zlist
    /*free memory of all items from the query list
        query_t it;
        while(zlist_size (query_list) > 0){
        	it = zlist_pop(query_list);
        	free(it.UID);
        	free(it.msg);
        }*/
    //zlist_destroy(send_msgs);
    //zlist_destroy(filter_list);

    zpoller_destroy (&poller);

    zyre_stop (remote);
    zyre_stop (local);

    zyre_destroy (&remote);
    zyre_destroy (&local);
    //  @end
    printf ("SHUTDOWN\n");
    return 0;
}
