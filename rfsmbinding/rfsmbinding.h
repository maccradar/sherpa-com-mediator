/*
 * RFSM API C-binding 
 * 
 * Simple binding to call RFSM 
 * 
 * Author: Enea Scioni
 * email: <enea.scioni@unife.it>
 * University of Leuven, Belgium
 * 
 */

#ifndef _RFSM_BINDING_H_
#define _RFSM_BINDING_H_

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>

/* RFSM-API */
int rfsm_init_fsm(lua_State* L, char* filename);
int rfsm_run(lua_State* L);
int rfsm_step(lua_State* L);
int rfsm_steps(lua_State* L, int num_steps);
int rfsm_sendevent(lua_State* L, char* event);
int rfsm_sendevents(lua_State* L, char* events[], unsigned int esize);

/* Internal */
void l_message (const char *pname, const char *msg);
int report (lua_State *L, int status);
int load_library(lua_State* L, char* filename);


#endif