
#include <zyre.h>
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <mediator.h>




/*
///////////////////////////////////////////////////
// create team
 *
 * DEPRECATED!
 *


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
void query_remote_file(mediator_t *self, json_msg_t *msg) {
	/**
	 * fetches a file from a remote location
	 *
	 * @param mediator_t* to the mediator data strucure
	 * @param json_msg_t* to the decoded zyre msg
	 */
	json_t *pl;
	json_error_t error;
	pl= json_loads(msg->payload,0,&error);
	if(!pl) {
		printf("Error parsing JSON payload! line %d: %s\n", error.line, error.text);
		json_decref(pl);
		return;
	}
	char *uri = NULL;
    if (json_object_get(pl,"URI")) {
    	uri = strdup((char*)json_string_value(json_object_get(pl,"URI")));
	} else {
		printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
		return;
	}
    printf("[%s] query remote file with URI: %s\n", self->shortname,uri);
    if (!uri) {
       printf("[%s] URI not specified, ignoring query!\n", self->shortname); 
       return;
    }
    const char s[2] = ":";
    char *token;
    token = strtok(uri, s);
    char* peerid = strdup(token);
    while( token != NULL ) { 
      token = strtok(NULL, s);
    }
    printf("[%s] Sending whisper to %s\n", self->shortname, peerid);
    zyre_whispers(self->remote, peerid, "%s", encode_msg("sherpa_mgs","http://kul/query_remote_file.json","query_remote_file",pl));
    json_decref(pl);
    
}

///////////////////////////////////////////////////
// get mediator uuid
char* generate_mediator_uuid(mediator_t *self, json_msg_t *msg) {
    /**
     * generates a msg containing the uuid of the mediator in the local (on robot) and remote (intra robot) zyre network
     *
     * @param mediator_t* pointer to struct containing all the info about the mediator
     * @param json_msg_t* to the decoded zyre msg
     *
     * @return returns NULL if it fails and a json object with the query ID, the local and remote uuid of the mediator
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
	if (json_object_get(pl,"UID")) {
		json_object_set(payload, "UID", json_object_get(pl,"UID"));
	} else {
		printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
		return ret;
	}
	// get and add remote uuid
	json_object_set(payload, "remote", json_string(zyre_uuid(self->remote)));
	// get and add local uuid
	json_object_set(payload, "local", json_string(zyre_uuid(self->local)));

	ret = encode_msg("sherpa_mgs","http://kul/mediator_uuid.json","mediator_uuid",payload);
	json_decref(payload);
	json_decref(pl);
	return ret;
}
///////////////////////////////////////////////////
// remote peer query

char* generate_peer_list(mediator_t *self, json_msg_t *msg) {
    /**
     * generates a list list of peers connected on the given zyre network
     *
     * @param mediator_t* pointer to struct containing all the info about the mediator
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
    if (json_object_get(pl,"UID")) {
    	json_object_set(payload, "UID", json_object_get(pl,"UID"));
	} else {
		printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
		return ret;
	}
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
void send_remote(mediator_t *self, json_msg_t *result, const char* group) {
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
	if (json_object_get(send_rqst,"recipients")) {
		recipients = json_object_get(send_rqst,"recipients");
	} else {
		printf("[%s] WARNING: No query recipients given! Will abort. \n", self->shortname);
		json_decref(send_rqst);
		return;
	}
	if (!json_is_array(recipients)) {
		printf("[%s] recipients of requested communication are not a JSON array!",self->shortname);
		json_decref(send_rqst);
		return;
	}
	//TODO: validate if payload is proper Sherpa msg
	//TODO: use payload type to allow binary payload
	const char *type = NULL;
	if (json_object_get(send_rqst,"payload_type")) {
		type = json_string_value(json_object_get(send_rqst,"payload_type"));
	} else {
		printf("[%s] WARNING: No payload_type given! Will abort. \n", self->shortname);
		json_decref(send_rqst);
		return;
	}
	if (!type) {
		printf("[%s] could not find payload_type!",self->shortname);
		json_decref(send_rqst);
		return;
	}
	json_t *pl;
	if (json_object_get(send_rqst,"payload")) {
		pl = json_object_get(send_rqst,"payload");
	} else {
		printf("[%s] WARNING: No payload given! Will abort. \n", self->shortname);
		json_decref(send_rqst);
		return;
	}
	if (!pl) {
		printf("[%s] could not find payload!",self->shortname);
		json_decref(send_rqst);
		return;
	}
	printf("#recipients: %zu \n", json_array_size(recipients));
	if (json_array_size(recipients) == 0) {
		printf("[%s] No recipients. Fire and forget msg.\n",self->shortname);
		char *res = (char*) malloc(sizeof(char)*(strlen("http://kul/")+strlen(type)+strlen(".json")+10));
		assert(res);
		strcpy(res,"http://kul/");
		strcat(res,type);
		strcat(res,".json");
		zyre_shouts(self->remote, group, "%s", encode_msg("sherpa_mgs",res,type,send_rqst));
		printf("sending %s \n",json_dumps(send_rqst, JSON_ENCODE_ANY));
		json_decref(send_rqst);
		//if (res) {free(res);}
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
			recipient_t *rec = (recipient_t*)malloc(sizeof(recipient_t));
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
			zyre_whispers(self->local, json_string_value(json_object_get(send_rqst,"local_requester")), "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
			json_decref(tmp);
			json_decref(pl);
		} else {
                        send_msg_request_t *msg_req = (send_msg_request_t*)malloc(sizeof(send_msg_request_t));;
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
			dummy = json_object_get(send_rqst,"local_requester");
			if ((!dummy)||(!json_is_string(dummy))) {
				printf("[%s] could not find requester in send_request \n",self->shortname);
				zlist_destroy(&peers);
				json_decref(unknown_recipients);
				json_decref(send_rqst);
				return;
			}
			msg_req->local_requester = json_string_value(dummy);
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
			int64_t ts = zclock_usecs ();
			if (ts < 0) {
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
	//load the payload=send_request
	if(!req) {
		printf("Error parsing JSON payload!\n");
		return;
	} else {
		json_t *rec = NULL;
		if (json_object_get(req,"recipients")) {
			rec = json_object_get(req,"recipients");
		} else {
			printf("[%s] WARNING: No recipients given! Will abort. \n", self->shortname);
			json_decref(req);
			return;
		}
		if(!json_is_array(rec)) {
			printf("[%s] receivers is not a JSON array!", self->shortname);
		} else {
			size_t index;
			json_t *value;
			//check if our robot is in the list of recipients and if yes, send ack
			json_array_foreach(rec, index, value) {
				if (streq(json_string_value(value),zyre_uuid(self->remote))) {
					json_t *pl;
					pl = json_object();
					if (json_object_get(req,"UID")) {
						json_object_set(pl, "UID", json_object_get(req,"UID"));
					} else {
						printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
						json_decref(pl);
						json_decref(req);
						return;
					}
					json_object_set(pl, "ID_receiver", json_string(zyre_uuid(self->remote)));
                                        // zyre_whispers(self->local, peerid, "%s", encode_msg("sherpa_mgs","http://kul/communication_ack.json","communication_ack",pl));
					zyre_whispers(self->remote, peerid, "%s", encode_msg("sherpa_mgs","http://kul/communication_ack.json","communication_ack",pl));

                                        json_decref(pl);
					break;
				}
			}
		}
		json_decref(rec);
		// filter by msg requester+uid to see if this msg has already been forwarded to local network
		filter_list_item_t *it = zlist_first(self->filter_list);
		int flag = 0;
		if (!json_object_get(req,"UID")) {
			printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
			return;
		}
		while (it != NULL) {
			if (streq(it->sender,peerid) && streq(it->msg_UID,json_string_value(json_object_get(req,"UID")))) {
				flag = 1;
				break;
			}
			it = zlist_next(self->filter_list);
		}
		if (flag == 0) {
			// if not in list, forward msg to local network
			printf("forwarding payload to local network \n");
			if(!json_object_get(req,"payload")) {
				printf("[%s] WARNING: No payload given! Will abort. \n", self->shortname);
				return;
			}
			zyre_shouts(self->local, self->localgroup, "%s", json_dumps(json_object_get(req,"payload"), JSON_ENCODE_ANY));
			// push this msg into filter list
			filter_list_item_t *tmp = (filter_list_item_t *) zmalloc (sizeof (filter_list_item_t));
			if (json_object_get(req,"UID")) {
				tmp->msg_UID = strdup(json_string_value(json_object_get(req,"UID")));
			} else {
				printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
				return;
			}
			tmp->sender = strdup(peerid);
			assert(tmp->sender);
		    int64_t ts = zclock_usecs();
			if (ts < 0) {
				printf("[%s] Could not assign time stamp!\n",self->shortname);
				return;
			} else {
				tmp->ts = ts;
				zlist_push(self->filter_list,tmp);
			}
			printf("adding msg to filter list\n");
		}
	}
	//json_decref(req);
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
			printf("handling remote send\n");
			handle_remote_send_remote(self, result, peerid);
		}
	} else {
		printf ("[%s] message could not be decoded\n", self->shortname);
	}
	zstr_free(&message);
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
					if (json_object_get(root,"UID")) {
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
					} else {
						printf("[%s] WARNING: No URI given! Will abort. \n", self->shortname);
						return;
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
				const char* uid = NULL;
				if (json_object_get(req,"UID")) {
					uid = json_string_value(json_object_get(req,"UID"));
				} else {
					printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
					return;
				}
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
				rc = zhash_insert (self->queries, uid, file_server);

				// Add to remote query_list
				query_t * q = query_new(uid, peerid, result, file_server);
				zlist_append(self->remote_query_list, q);
  				zpoller_add(self->poller, file_server);
				json_t *pl;
				pl = json_object();
				json_object_set(pl, "UID", json_object_get(req,"UID"));
				json_object_set(pl, "URI", json_string(endpoint));
				printf("[%s] whispering server endpoint %s to peer %s\n", self->shortname,endpoint, peerid);
				zyre_whispers(self->remote, peerid, "%s", encode_msg("sherpa_mgs","http://kul/endpoint.json","endpoint",pl));
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
				const char* uid = NULL;
				if (json_object_get(req,"UID")) {
					uid = json_string_value(json_object_get(req,"UID"));
				} else {
					printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
					return;
				}
				int rc;
				const char *args[3];
				args[0] = strdup(peerid);
  				args[1] = strdup(uid);
				args[2] = json_string_value(json_object_get(req, "URI"));
                                zactor_t * file_client = zactor_new (client_actor, args);
                                rc = zhash_insert (self->queries, uid, file_client);
                                // Required to know when transfer is completed
  				zpoller_add(self->poller, file_client);
			}
		} else if (streq (result->type, "remote_file_done")) {
			json_t *req;
			json_error_t error;
			req = json_loads(result->payload, 0, &error);
			if(!req) {
				printf("Error parsing JSON payload!\n");
				return;
			} else {
				const char* uid = NULL;
				if (json_object_get(req,"UID")) {
					uid = json_string_value(json_object_get(req,"UID"));
				} else {
					printf("[%s] WARNING: No query URI given! Will abort. \n", self->shortname);
					return;
				}
				printf("[%s] received remote_file_done, killing server %s\n", self->shortname, uid);
				zactor_t *file_server = (zactor_t*) zhash_lookup(self->queries, uid);
  				zpoller_remove(self->poller, file_server);
				zactor_destroy(&file_server); // TODO: Does this send "$TERM"?
                                zhash_delete (self->queries, uid);
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

void handle_remote_evasive (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 2);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	printf ("[%s] EVASIVE %s %s\n", self->shortname, peerid, name);
	zstr_free(&peerid);
	zstr_free(&name);
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
		} else if (streq (result->type, "send_request")) {
			// query for communication
			send_remote(self, result, self->remotegroup);
		} else if (streq (result->type, "query_mediator_uuid")) {
			// send uuid of local (gossip) and remote network (to be used )
			char *mediator_uuid_msg = generate_mediator_uuid(self, result);
			if (mediator_uuid_msg) {
				//zyre_whispers(self->local, peerid, "%s", mediator_uuid_msg);
				zyre_shouts(self->local,self->localgroup, "%s", mediator_uuid_msg);
			} else {
				printf ("[%s] Could not generate mediator uuid! \n", self->shortname);
			}
			zstr_free(&mediator_uuid_msg);
		} else if (streq (result->type, "query_remote_file")) {
			json_t *req;
			json_error_t error;
			req = json_loads(result->payload, 0, &error);
			if(!req) {
				printf("Error parsing JSON payload!\n");
				return;
			} else {
				const char* uid = json_string_value(json_object_get(req,"UID"));
                query_t * q = query_new(uid, strdup(peerid), result, NULL);
				zlist_append(self->local_query_list, q); 
				query_remote_file(self, result);
			}
		} else {
			printf("[%s] Unknown msg type!",self->shortname);
		}
	} else {
		printf ("[%s] message could not be decoded\n", self->shortname);
	}
	free(result);
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

void handle_local_evasive (mediator_t *self, zmsg_t *msg) {
	assert (zmsg_size(msg) == 2);
	char *peerid = zmsg_popstr (msg);
	char *name = zmsg_popstr (msg);
	printf ("[%s] EVASIVE %s %s\n", self->shortname, peerid, name);
	zstr_free(&peerid);
	zstr_free(&name);
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
		zyre_whispers(self->local, it->local_requester, "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
		json_decref(pl);
		send_msg_request_t *dummy = it;
		it = zlist_next(self->send_msgs);
		zlist_remove(self->send_msgs,dummy);
	} else {
		int64_t curr_time = zclock_usecs ();
		if (curr_time > 0) {
			// if timeout, send report and remove item from list
			double curr_time_msec = curr_time*1.0e-3;
			double ts_msec = it->ts_added*1.0e-3;
			if (curr_time_msec - ts_msec > it->timeout) {
				json_t *pl;
				pl = json_object();
				json_object_set(pl, "UID", json_string(it->uid));
				json_object_set(pl, "success", json_false());
				json_object_set(pl, "error", json_string("Timeout"));
				json_object_set(pl, "recipients_delivered", acknowledged);
				json_object_set(pl, "recipients_undelivered", unacknowledged);
				zyre_whispers(self->local, it->local_requester, "%s", encode_msg("sherpa_mgs","http://kul/communication_report.json","communication_report",pl));
				json_decref(pl);
				send_msg_request_t *dummy = it;
				it = zlist_next(self->send_msgs);
				zlist_remove(self->send_msgs,dummy);
			} else {
				double ts_msec = it->ts_last_sent*1.0e-3;
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
		int64_t curr_time = zclock_usecs ();
		if (curr_time > 0) {
			filter_list_item_t *it = zlist_first(self->filter_list);
			int length = json_integer_value(json_object_get(self->config, "msg_filter_length"));
			while (it != NULL) {
				double curr_time_msec = curr_time*1.0e-3;
				double ts_msec = it->ts*1.0e-3;
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
    printf("[%s] mediator initialised!\n", self->shortname);
    
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
            } else if (streq (event, "EVASIVE")) {
            	handle_local_evasive (self, msg);
            } else {
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
            } else if (streq (event, "EVASIVE")) {
	        handle_remote_evasive (self, msg);
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
            if (query != NULL) {
		// TODO: use JSON for internal communication?
        	char *query_type = zstr_recv (which);
        	if (streq (query_type, "remote_file_done")) { // TODO: how to call it? Query causing this file client is called "endpoint"
			char *peerid = zstr_recv (which);
			char *recv_uid = zstr_recv (which);
			char *file_path = zstr_recv (which);
 			assert(streq(uid, recv_uid));
     			printf("[%s] received remote_file_done from client_actor\n", self->shortname);
  			zpoller_remove(self->poller, query);
			zhash_delete(self->queries,uid);
			//zactor_destroy (&query); // TODO: required?
			json_t *pl;
			pl = json_object();
			json_object_set(pl, "UID", json_string(recv_uid));
			printf("[%s] whispering remote peerid %s that query %s is done\n", self->shortname, peerid, recv_uid);
			zyre_whispers(self->remote, peerid , "%s", encode_msg("sherpa_mgs","http://kul/remote_file_done.json","remote_file_done",pl));

			json_object_set(pl, "URI", json_string(file_path));
			// look up local requester
			query_t *q = (query_t *) zlist_first(self->local_query_list);
			char* requester = NULL;
			while (q != NULL) {
				if (streq(q->uid, recv_uid)) {
					requester = strdup(q->requester);
					break;
				}
				q = (query_t *) zlist_next(self->local_query_list);
			}
			if(requester != NULL) {
				printf("[%s] whispering local peerid %s that file is located at %s\n", self->shortname, requester, file_path);
				zyre_whispers(self->local, requester, "%s", encode_msg("sherpa_msgs", "http://kul/file_path.json", "file_path", pl));
			} else {
				printf("[%s] requester of local query %s not found!\n", self->shortname, recv_uid);
			}
			free (recv_uid);
			json_decref(pl);
		}
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



