/*
 * This file is part of the Pick-n-Pack reference implementation
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

/*! \brief Computation function for LCSM creating state of the Line.
    This function creates the necessary data structures for the Line's LCSM. It
    assumes the process in which this role is fulfilled already created a resource_t data
    structure
    \param self pointer to resource_t data structure. 
 */
int creating(resource_t *self) {
    printf("[%s] creating...", self->name);
    self->frontend =  NULL;
    self->backend =  NULL;
    self->backend_resources = zlist_new();
    self->required_resources = zlist_new();
    self->com =  NULL;
    self->input = NULL;
    self->userinput = NULL;
    self->configured_resources = false;
    self->deleted = false;
    self->frontend_connected = false;
    printf("...done.\n");

    return 0;
}

/*! \brief Computation function for LCSM configuring_resources state of the Line.
    This function configures the necessary resources, i.e. communication channels, for 
    the Line's LCSM.
    \param self pointer to resource_t data structure. 
 */
int configuring_resources(resource_t* self) {
    if(self->configured_resources)
	return 0;
    printf("[%s] configuring resources...", self->name);
    // send signal on pipe socket to acknowledge initialization
    zsock_signal (self->pipe, 0);
    zlist_push(self->required_resources, PNP_QAS_ID);
    zlist_push(self->required_resources, PNP_PRINTING_ID);
    
    // configuring frontend and backend sockets
    self->frontend = zsock_new_dealer(self->frontend_str);
    self->backend =  zsock_new_router (self->backend_str);
    self->com =  zpoller_new (self->pipe, self->frontend, self->backend, NULL);
    self->configured_resources = true;
    printf("done.\n");

    return 0;

}

/*! \brief Computation function for LCSM configuring_capabilities state of the Line.
    This function configures specific parameters to fullfil the role of a Line.
    \param self pointer to resource_t data structure. 
 */
int configuring_capabilities (resource_t* self) {
    printf("[%s] configuring capabilities...", self->name);
    //  If liveness hits zero, queue is considered disconnected
    self->liveness = HEARTBEAT_LIVENESS;
    self->interval = INTERVAL_INIT;

    //  Send out heartbeats at regular intervals
    self->heartbeat_at = zclock_time () + HEARTBEAT_INTERVAL;

    srandom ((unsigned) time (NULL));
    // Send ready signal to frontend to announce itself
    json_error_t error;
    json_t *module_ready = json_load_file("line_ready.json", JSON_REJECT_DUPLICATES, &error);
    if(!module_ready) {
        printf("[%s] error parsing JSON string! line %d: %s\n", self->name, error.line, error.text);
        return 0;
    }
    if(!json_is_array(module_ready)) {
        printf("[%s] error: root is not an array\n", self->name);
        json_decref(module_ready);
        return 0;
    }
    char* json_string = json_dumps(module_ready,JSON_ENSURE_ASCII);
    zframe_t *frame = zframe_new (json_string, strlen(json_string));
    zframe_send(&frame, self->frontend, 0);
    printf("done.\n");
    return 0;
}

/*! \brief Computation function for LCSM running state of the Line Role.
    This function contains the running loop of the Line Role. It has the logic of the stop-light
    controller, which is switched depending on backend and/or frontend events.
    \param self pointer to resource_t data structure. 
 */
int running(resource_t *self) {
    printf("[%s] RUNNING\n", self->name);
    
    // Send stoplight commands
    backend_resource_t *backend_resource = (backend_resource_t *) zlist_first (self->backend_resources);
	while (backend_resource) {
		zframe_send (&backend_resource->identity, self->backend, ZFRAME_REUSE + ZFRAME_MORE);
		int light = rand() % 3;
		/*zframe_t *frame = zframe_new(PNP_GREEN, FRM_LEN);
		if(light == 1) 
		    frame = zframe_new (PNP_ORANGE, FRM_LEN);
		if(light == 2)
		    frame = zframe_new (PNP_RED, FRM_LEN);
		zframe_send (&frame, self->backend, 0);
		*/
printf("[%s] TX %i BACKEND %s\n", self->name, light, backend_resource->id_string);
		backend_resource = (backend_resource_t *) zlist_next (self->backend_resources);
	}
    
    return 0;
}

/*! \brief Computation function for LCSM pausing state of the Line Role.
    This function should contain all activity that has to be executed when the Line
    is in a pausing state.
    \param self pointer to resource_t data structure. 
 */
int pausing(resource_t *self) {
    printf("[%s] pausing...", self->name);
    printf("done.\n");
    return 0;
}

/*! \brief Computation function for LCSM deleting state of the Line Role.
    This function cleans up all allocated data structures for the LCSM of the Line Role.
    \param self pointer to resource_t data structure. 
 */
int deleting(resource_t *self) {
    if(self->deleted)
	return 0;
    printf("[%s] deleting...", self->name);
    //  When we're done, clean up properly
    while (zlist_size (self->backend_resources)) {
    	backend_resource_t *backend_resource = (backend_resource_t *) zlist_pop (self->backend_resources);
        s_backend_resource_destroy (&backend_resource);
    }
    zsock_destroy(&self->frontend);
    zsock_destroy(&self->backend);
    zpoller_destroy (&self->com);
    self->frontend = NULL;
    self->backend = NULL;
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
    } else if (streq (self->userinput, "G")) {
	return "e_green";
    } else if (streq (self->userinput, "O")) {
	return "e_orange";
    } else if (streq (self->userinput, "R")) {
	return "e_red";
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
	void *which = zpoller_wait (self->com, HEARTBEAT_INTERVAL * ZMQ_POLL_MSEC);
	
        /* 2.1 Input received from main loop? E.g. keyboard input. */
	if (which == self->pipe) {
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self->name); 
	        return;
            }
            self->userinput = zmsg_popstr (msg);
            printf("[%s] received keyboard input: %s\n", self->name, self->userinput);
	    self->userinput = parse_userinput(self);
	    printf("[%s] new event: %s\n", self->name, self->userinput);
  	    zmsg_destroy (&msg);
        }	
	/* 2.2 Input received from frontend? E.g. plant. */
	if (which == self->frontend) {
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self->name); 
	        return;
            }
	    printf("[%s] received frontend input\n", self->name);	    
            /* 2.b Pop that single frame of the message into a fresh string and validate it's JSON */
	    self->input = zmsg_popstr(msg);
            decode_json(self);
	    self->liveness = HEARTBEAT_LIVENESS;
  	    zmsg_destroy (&msg);
	}
	/* 2.3 Input received from backend? E.g module. */
	if (which == self->backend) {
            zmsg_t *msg = zmsg_recv (which);
	    if (!msg) {
	        printf("[%s] interrupted!\n", self->name); 
	        return;
            }
	    printf("[%s] received backend input\n", self->name);	    
	    //  Any sign of life from backend_resource means it's ready
            zframe_t *identity = zmsg_unwrap (msg);
	    self->input = zmsg_popstr(msg);
            decode_json(self);
  	    
	    /* Iterate over input events and check for READY adn HEARTBEAT events */
  	    event_t *e = (event_t *) zlist_first (self->input_events);
  	    char* name = NULL;
	    while (e) {
         	printf("[%s] checking event %s\n", self->name, e->event_id);
		if(streq(e->event_id, PNP_HEARTBEAT)) {
		    printf("[%s] BE RX HB event FROM %s\n", self->name, e->model_id);
		    zlist_remove(self->input_events, e);
 	    	    name = e->model_id;
	    	}
                if(streq(e->event_id, PNP_READY)) {
		    printf("[%s] BE RX READY EVENT FROM %s\n", self->name, e->model_id);
		    zlist_remove(self->input_events, e);
		    name = e->model_id;
		}
     	        e = (event_t *) zlist_next (self->input_events);
   	    }
	    if(name != NULL) {
	    	backend_resource_t *backend_resource = s_backend_resource_new (identity, name);
	    	s_backend_resource_ready (backend_resource, self->backend_resources);
  	    }
	    /* We don't need the message anymore */
	    zmsg_destroy (&msg);
	}
	
        //else  
	//  If the queue hasn't sent us heartbeats in a while, destroy the
	//  socket and reconnect. This is the simplest most brutal way of
	//  discarding any messages we might have sent in the meantime:
	//if (--self->liveness == 0) {
	//	printf ("[%s] FE heartbeat failure, can't reach frontend\n", self->name);
	//	printf ("[%s] FE reconnecting in %zd msec...\n", self->name, self->interval);
	//	zclock_sleep (self->interval);

	//	if (self->interval < INTERVAL_MAX)
	//		self->interval *= 2;
	//	zsock_destroy(&self->frontend);
	//	self->frontend = zsock_new_dealer(self->frontend_str); // TODO: this should be configured.
	//			self->liveness = HEARTBEAT_LIVENESS;
	//}
          
	/* 3. Process output */
        /* 4. Send heartbeats if required */
	if (zclock_time () >= self->heartbeat_at) {
            char* hb = generate_json_heartbeat(self->name);
            zframe_t *frame = zframe_new (hb, strlen(hb));
            backend_resource_t *backend_resource = (backend_resource_t *) zlist_first (self->backend_resources);
	    while (backend_resource) {
		zframe_send (&backend_resource->identity, self->backend, ZFRAME_REUSE + ZFRAME_MORE);
                zframe_send (&frame, self->backend, 0);
		printf("[%s] BE TX HB %s:%s\n", self->name, backend_resource->id_string, hb);
		backend_resource = (backend_resource_t *) zlist_next (self->backend_resources);
	    }
		
	s_backend_resources_purge (self->backend_resources);
	    
            // Send heartbeat to frontend
	    zframe_send(&frame, self->frontend, 0);
            printf("[%s] FE TX HB: %s \n", self->name, hb);
	    self->heartbeat_at = zclock_time () + HEARTBEAT_INTERVAL;
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
//    if(streq(str,"red"))
//	red(self);
//    if(streq(str,"green"))
//	green(self);
//    if(streq(str,"orange"))
//	orange(self);
}

/*! \brief Logging function, which is part of the running loop.
    [LOGGING]	 	log all data that has been changed in this step. Basically, this means logging the resource_t state.
 */
void logging(resource_t *self) {
}

/*! \brief The resource_actor thread triggers communication, coordination, configuration, scheduling and logging.
    This function contains the running loop of this process:
    
    [COMMUNICATION] 	check communication channels and parse any messages into events.
    [COORDINATION] 	push events to the role's LCSM and step the FSM and 
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
    //printf("[%s] actor started.\n", name);
    // creating state of LCSM
    resource_t *self = (resource_t *) zmalloc (sizeof (resource_t));
    self->name = name;
    self->pipe = pipe;
    self->com = NULL;
    self->frontend_str = (char*)argv[3];
    self->backend_str = (char*)argv[4];
    self->input_events = zlist_new();
    self->output_events = zlist_new();
    printf("[%s] actor started for with frontend %s and backend %s\n", name, self->frontend_str, self->backend_str);
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
    Usage: ./line myname myfsm
    \param myname give this process a name
    \param myfsm provide a lua file containing the LCSM model for this process

    Its purpose is to:
    1. launch an actor thread running the application
    2. monitor keyboard input and send it to the resource actor
    3. monitor system interrupts and gracefully stop and destroy the resource actor
*/
int main(int argc, char *argv[])
{
    /* Usage */
    if (argc < 6) {
        puts ("syntax: ./line myfsm myname frontend_port backend_port proto_port");
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
  
  /*  if(load_library(L, "rfsm")!=0) {
    	printf("[%s] error loading rfsm library, aborting...\n", argv[2]);
     	goto out;
    }
  
    if(rfsm_init_fsm(L, fsm)!=0) {
    	printf("[%s] error initializing fsm\n", argv[2]);
    	goto out;
    }
    // successfully opened LCSM lua file
*/
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
	zstr_sendx (actor, message, NULL);
    }
    // System interrupt received. Close actor thread and LCSM
    printf("[%s] received interrupt, aborting...\n", argv[2]);
    zactor_destroy (&actor);

out:
    lua_close(L);
    return 0;
}
