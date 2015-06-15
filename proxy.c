/*
 * This file is part of the communication proxy implementation
 *
 * Copyright (c) 2015, Department of Mechanical Engineering, KU Leuven, Belgium.
 * All rights reserved.
 *
 * You may redistribute this software and/or modify it under the terms of the
 * "Simplified BSD License" (BSD 2-Clause License).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1.	Redistributions of source code must retain the above copyright notice, 
 *    	this list of conditions and the following disclaimer.
 *
 * 2. 	Redistributions in binary form must reproduce the above copyright notice,
 *	this list of conditions and the following disclaimer in the documentation
 *	and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include "defs.h"

/*! \brief Computation function for LCSM creating state of the Proxy.
    This function creates the necessary data structures for the Proxy's LCSM.
    \param self pointer to resource_t data structure. 
 */
int creating(resource_t *self) {
    printf("[%s] creating...", self->name);
    self->node =  NULL;
    self->com =  NULL;
    self->input = NULL;
    self->userinput = NULL;
    self->configured_resources = false;
    self->deleted = false;
    printf("...done.\n");

    return 0;
}

/*! \brief Computation function for LCSM configuring_resources state of the Proxy.
    This function configures the necessary resources, i.e. communication channels,
    for the Proxy's LCSM.
    \param self pointer to resource_t data structure. 
 */
int configuring_resources(resource_t* self) {
    if(self->configured_resources)
	return 0;
    printf("[%s] configuring resources...", self->name);
    // send signal on pipe socket to acknowledge initialization
    zsock_signal (self->pipe, 0);
    self->node = zyre_new(self->name);
    zyre_set_header(self->node,"MODEL", "%s", self->model_uri); 
    //zyre_set_verbose(self->node);    
    self->com =  zpoller_new (self->pipe, zyre_socket(self->node), NULL);
    self->configured_resources = true;
    printf("done.\n");

    return 0;

}

/*! \brief Computation function for LCSM configuring_capabilities state of the Proxy.
    This function configures specific parameters to fullfil the role of a Proxy.
    \param self pointer to resource_t data structure. 
 */
int configuring_capabilities (resource_t* self) {
    printf("[%s] configuring capabilities...", self->name);
    zyre_start(self->node);   
    zyre_join(self->node, self->group);   
    return 0;
}

/*! \brief Computation function for LCSM running state of the Proxy.
    This function contains the running loop of the Proxy.
    \param self pointer to resource_t data structure. 
 */
int running(resource_t *self) {
    printf("[%s] running...\n", self->name);
    return 0;
}

/*! \brief Computation function for LCSM pausing state of the Proxy.
    This function should contain all activity that has to be executed when the Proxy is in a pausing state.
    \param self pointer to resource_t data structure. 
 */
int pausing(resource_t *self) {
    printf("[%s] pausing...", self->name);
    printf("done.\n");
    return 0;
}

/*! \brief Computation function for LCSM deleting state of the Proxy.
    This function cleans up all allocated data structures for the LCSM of the Proxy.
    \param self pointer to resource_t data structure. 
 */
int deleting(resource_t *self) {
    if(self->deleted)
	return 0;
    printf("[%s] deleting...", self->name);
    //  When we're done, clean up properly
    zpoller_destroy (&self->com);
    //zyre_stop(self->node);
    //zclock_sleep(100);
    zyre_destroy(&self->node);
    
    self->deleted = true;
    printf("done.\n");
    return 0;
}

/*! \brief Parser for messages received on any of the communication channels.
    This function parses input received on the frontend, backend or pipe socket
    and returns the appropriate event for the LCSM.
    \param self pointer to resource_t data structure. 
 */
char* parse_userinput(resource_t *self) {
    if (streq (self->userinput, "$TERM")) {
        return "e_quit";
    } else if (streq (self->userinput, "step")) {
	return "e_step";
    } else {
        //printf("[%s] added event: %s\n", self->name, message);
    }
    return self->userinput;
}

void decode_json(resource_t *self) {
    json_t *root;
    json_error_t error;
    root = json_loads(self->input, 0, &error);
    
    if(!root) {
   	printf("[%s] error parsing JSON string! line %d: %s\n", self->name, error.line, error.text);
    	return;
    }
    if(!json_is_array(root)) {
        printf("[%s] error: root is not an array\n", self->name);
        json_decref(root);
        return;
    }
    int i;
    for(i = 0; i < json_array_size(root); i++) {
        event_t *e = (event_t *) zmalloc (sizeof (event_t));
        json_t *data, *timestamp, *event, *metamodel_id, *model_id, *event_id;
        const char *metamodel_id_text, *model_id_text, *event_id_text;

        data = json_array_get(root, i);
        if(!json_is_object(data))
        {
            printf("[%s] error: event data %d is not an object\n", self->name, (int)(i + 1));
            json_decref(root);
            return;
        }

        timestamp = json_object_get(data, "timestamp");
        if(!json_is_string(timestamp)) {
            printf("[%s] error: event %d: timestamp is not a string\n", self->name, (int)(i + 1));
            json_decref(root);
	    return;
        }

        event = json_object_get(data, "event");
        if(!json_is_object(event)) {
            printf("[%s] error: event %d: event is not an object\n", self->name, (int)(i + 1));
            json_decref(root);
            return;
        }

        metamodel_id = json_object_get(event, "metamodel_id");
        if(!json_is_string(metamodel_id)) {
            printf("[%s] error: event %d: metamodel_id is not a string\n", self->name, (int)(i + 1));
            json_decref(root);
            return;
        }

        model_id = json_object_get(event, "model_id");
        if(!json_is_string(model_id)) {
            printf("[%s] error: event %d: model_id is not a string\n", self->name, (int)(i + 1));
            json_decref(root);
            return;
        }

        event_id = json_object_get(event, "event_id");
        if(!json_is_string(event_id)) {
            printf("[%s] error: event %d: event_id is not a string\n", self->name, (int)(i + 1));
            json_decref(root);
            return;
        }
        e->timestamp = strdup(json_string_value(timestamp));
        e->metamodel_id = strdup(json_string_value(metamodel_id));
	e->model_id = strdup(json_string_value(model_id));
        e->event_id = strdup(json_string_value(event_id));
 
        printf("[%s] JSON parser: (%s) - %s - %s - %s\n", self->name, e->timestamp, e->metamodel_id, e->model_id, e->event_id);
        zlist_push(self->input_events, e);
    }
    json_decref(root);
}

/*! \brief Communication function, which is part of the running loop.
    
    [COMMUNICATION]	push any events for external actors to the respective communication channels.
 */
void communication(resource_t *self) {
	/* 1. Is communication ready? */
	if(self->com == NULL) {
		printf("[%s] no communication channels yet\n", self->name);
		return;
	}
	
	/* 2. Process input */ 
	void *which = zpoller_wait (self->com, ZMQ_POLL_MSEC);
	
        /* 2.1 Input received from main loop? E.g. keyboard input. */
	if (which == self->pipe) {
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self->name); 
	        return;
            }
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")) {
                self->userinput = command;
	        self->userinput = parse_userinput(self);
	        printf("[%s] new event: %s\n", self->name, self->userinput);
            } else if (streq (command, "SHOUT")) {
		char *group = zmsg_popstr (msg); 
                char *string = zmsg_popstr (msg);
                zyre_shouts (self->node, group, "%s", string);
            } else if (streq (command, "VERBOSE")) {
                zyre_set_verbose(self->node);
		printf("[%s] switch to verbose mode\n", self->name);
            } else if (streq (command, "WHISPER")) {
		char *peer = zmsg_popstr (msg); 
                char *string = zmsg_popstr (msg);
                zyre_whispers (self->node, peer, "%s", string);
            } else if (streq (command, "EVENT")) {
	    	char *string = zmsg_popstr (msg);
		self->userinput = string;
		self->userinput = parse_userinput(self);
	        printf("[%s] new event: %s\n", self->name, self->userinput);
            } else if (streq (command, "UUID")) {
		printf("[%s] my uuid is: %s\n", self->name, zyre_uuid(self->node));
	    } else if (streq (command, "NAME")) {
		printf("[%s] my name is: %s\n", self->name, zyre_name(self->node));
	    } else if (streq (command, "JOIN")) {
	        char *group = zmsg_popstr(msg);
		zyre_join(self->node, group);
         	printf("[%s] joined group: %s\n", self->name, group);
	    } else if (streq (command, "LEAVE")) {
	        char *group = zmsg_popstr(msg);
		zyre_leave(self->node, group);
         	printf("[%s] left group: %s\n", self->name, group);
	    } else if (streq (command, "PEERS")) {
		zlist_t *peers = zyre_peers(self->node);
		printf("[%s] my peers are:\n", self->name);
		char* peer = zlist_next(peers);
		while(peer != NULL) {
		    printf("[%s] %s\n", self->name, peer);
		    peer = zlist_next(peers);
		}
		printf("[%s] end of peers\n", self->name);
	    } else if (streq (command, "OWNGROUPS")) {
		zlist_t *groups = zyre_own_groups(self->node);
		printf("[%s] my own groups are:\n", self->name);
		char* group = zlist_next(groups);
		while(group != NULL) {
		    printf("[%s] %s\n", self->name, group);
		    group = zlist_next(groups);
		}
		printf("[%s] end of own groups\n", self->name);
	    } else {
                printf ("[%s] invalid message from keyboard\n", self->name);
            }
            free (command);
            zmsg_destroy (&msg);
  	    zmsg_destroy (&msg);
        }	
        else if (which == zyre_socket (self->node)) {
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self->name); 
	        return;
            }
            char *event = zmsg_popstr (msg);
            char *peer = zmsg_popstr (msg);
            char *name = zmsg_popstr (msg);
            char *group = zmsg_popstr (msg);
            char *message = zmsg_popstr (msg);

            if (streq (event, "ENTER")) { 
                printf ("%s has joined.\n", name);
		char* model_uri = zyre_peer_header_value(self->node, peer, "MODEL");
		char* model = read_url(model_uri);
		printf ("%s has model %s\n", name, model_uri);
		printf ("%s\n", model);
            }
            else if (streq (event, "EXIT")) { 
                printf ("%s has left.\n", name);
            }
            else if (streq (event, "SHOUT")) { 
                printf ("%s: %s\n", name, message);
            }
	    else {
            	printf ("Message from node\n");
            	printf ("event: %s peer: %s  name: %s\n  group: %s message: %s\n", event, peer, name, group, message);
	    }
            free (event);
            free (peer);
            free (name);
            free (group);
            free (message);
            zmsg_destroy (&msg);
        }
}

/*! \brief Coordination function, which is part of the running loop.
    
    [COORDINATION] 	push events to the role's LCSM and step the FSM and 
   			check if stepping the FSM resulted in any state transition.
 */
void coordination(resource_t *self) {
  /* 1. Step the LCSM */
  rfsm_step(L);
  /* 2. Check user input and send it */
  if(self->userinput != NULL) {
	printf("[%s] Sending user input event to rFSM %s\n", self->name, self->userinput);
	rfsm_sendevent(L, self->userinput);
  	//rfsm_run(L);
	self->userinput = NULL; 
  }
  /* 3. Iterate over input events and check for LCSM events */
  event_t *e = (event_t *) zlist_first (self->input_events);
  while (e) {
        if(e->metamodel_id == "LCSM") {
		printf("[%s] processing LCSM event %s\n", self->name, e->event_id);
  		rfsm_sendevent(L, e->event_id);
		zlist_remove(self->input_events, e);
	}
     	e = (event_t *) zlist_next (self->input_events);
  }
}

/*! \brief Scheduling function, which is part of the running loop.
    
    [SCHEDULING]	schedule any computations required by state transition, execute 
    			computation.
    TODO: use Coordinator-Configurator pattern here!
 */
void scheduling(resource_t *self) {
    lua_getglobal(L, "current_state");
    const char *str = lua_tostring(L, lua_gettop(L));
    //printf("[%s] current state: %s\n", self->name, str);
    if(streq(str,"creating"))
	creating(self);
    if(streq(str,"configuring_resources"))
	configuring_resources(self);
    if(streq(str,"configuring_capabilities"))
	configuring_capabilities(self);
    if(streq(str,"running"))
	running(self);
    if(streq(str,"deleting"))
	deleting(self);
}

/*! \brief Logging function, which is part of the running loop.
    [LOGGING]	 	log all data that has been changed in this step. Basically, this means logging the resource_t state.
 */
void logging(resource_t *self) {
    // If PUB socket is connected, send data to collector
    //if(self->pub)
    // Else send to stdout
    printf("[%s] log state...\n",self->name);
}

/*! \brief The resource_actor thread triggers communication, coordination, configuration, scheduling and logging.
    This function contains the running loop of this process:
    
    [COMMUNICATION] 	check communication channels and parse any messages into events.
    [COORDINATION] 	push events to the LCSM and step the FSM and 
   			check if stepping the FSM resulted in any state transition.
    [CONFIGURATION]	reconfigure if necessary based on state events from FSM.
    [SCHEDULING]	schedule any computations required by state transition, execute 
    			computation.
    [COORDINATION]	push new events, if any, to LCSM and step FSM again.
    [COMMUNICATION]	push any events for external actors to the respective communication channels.
    [LOGGING]	 	log all data that has been changed in this step. Basically, this means logging the resource_t state.
 */
static void resource_actor(zsock_t *pipe, void *args){
    char** argv = (char**) args; 
    char* name = (char*) argv[2];
    char* group = (char*) argv[3];
    //printf("[%s] actor started.\n", name);
    // creating state of LCSM
    resource_t *self = (resource_t *) zmalloc (sizeof (resource_t));
    self->name = name;
    self->group = group;
    self->pipe = pipe;
    self->com = NULL;
    self->input_events = zlist_new();
    self->output_events = zlist_new();
    self->model_uri = argv[4];
    // end creating state of LCSM

    // Running loop of this process:
    // 
    // [COMMUNICATION] 	check communication channels and parse any messages into events.
    // [COORDINATION] 	push events to the role's LCSM and step the FSM and 
    //			check if stepping the FSM resulted in any state transition.
    // [CONFIGURATION]	reconfigure if necessary based on state events from FSM.
    // [SCHEDULING]	schedule any computations required by state transition, execute 
    //			computation.
    // [COORDINATION]	push new events, if any, to LCSM and step FSM again.
    // [COMMUNICATION]	push any events for external actors to the respective communication channels.
    // [LOGGING] 	log all data that has been changed in this step. Basically, this means logging the resource_t state.
    //
    // Continue until deleted flag is set by the role's LCSM or system interrupt is received.

    while (!self->deleted && !zsys_interrupted) {
	//printf("[%s] actor loop\n", name);
	/*	[COMMUNICATION] 	*/
	communication(self);
	/*	[COORDINATION]		*/
	coordination(self);
	/*	[CONFIGURATION]		*/
	//configuration();
	/*	[SCHEDULING]		*/
	scheduling(self);
	/*	[COORDINATION]		*/
	//coordination();
	/*	[COMMUNICATION] 	*/
	//communication();
	/*	[LOGGING]		*/	
	//logging();
	sleep(1);
    }
    // if interrupted or deleted, run deleting state one last time
    deleting(self);
    printf("[%s] actor stopped.\n", name);
    // signal main thread that actor has stopped.
    zsock_signal(pipe, -1);
}


/*! \brief Main function is entry point to resource process.
    Its purpose is to:
    1. launch an actor thread running the application
    2. monitor keyboard input and send it to the resource actor
    3. monitor system interrupts and gracefully stop and destroy the resource actor
*/
int main(int argc, char *argv[])
{
    /* Usage */
    if (argc < 5) {
        puts ("syntax: ./proxy myfsm myname mygroup mymodel proto_port");
        exit (0);
    }
    // Create Lua state variable for the rFSM
    L = luaL_newstate();
    luaL_openlibs(L);  

    char *fsm;
    char *name;
    char *proto_port;
    fsm = argv[1];
    name = argv[2];
    proto_port = argv[5];
    
    lua_pushstring(L, fsm);
    lua_setglobal(L, "fsm_uri");
    lua_pushstring(L, name);
    lua_setglobal(L, "name");    
    lua_pushstring(L, proto_port);
    lua_setglobal(L, "proto_port");
  
    if (luaL_loadfile(L, "scripts/gen-rfsm.lua")) {
    	printf("Error load gen fsm script..check path!\n");
    	goto out;
    }
    lua_pcall(L,0,0,0);
    // successfully opened LCSM lua file
    
    // start actor thread to execute running loop of this process
    // the main thread will be used to receive keyboard input in parallel
    zactor_t *actor = zactor_new (resource_actor, argv);
    assert (actor);
    
    // receive keyboard input and pipe it to the actor thread
    while (!zsys_interrupted) {
        char message [1024];
        if (!fgets( message, 1024, stdin))
	        break;
	message[strlen(message)-1] = 0; // drop the trailing linefeed
	char * pch;
	pch = strtok (message," ");
  	while (pch != NULL) {
    	    printf ("Received: %s\n",pch);
	    zstr_sendm (actor, pch);
    	    pch = strtok (NULL, " ,.-");
  	}
	zstr_send(actor, NULL);
    }
    // System interrupt received. Close actor thread and LCSM
    printf("\n[%s] received interrupt, aborting...\n", argv[2]);
    zactor_destroy (&actor);

out:
    lua_close(L);
    return 0;
}
