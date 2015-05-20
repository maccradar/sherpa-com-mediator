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

#include "rfsmbinding.h"

void l_message (const char *pname, const char *msg) {
  if (pname) fprintf(stderr, "%s: ", pname);
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
}

int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message("callrfsm", msg);
    lua_pop(L, 1);
  }
  return status;
}

int load_library(lua_State* L, char* filename) {
  lua_getglobal(L, "require");
  lua_pushstring(L, filename);
  return report(L, lua_pcall(L, 1, 0, 0));
}

int rfsm_run(lua_State* L) {
  int ret = 0;
  lua_getglobal(L, "rfsm");
  lua_pushstring(L, "run");
  lua_gettable(L, -2); /* Get run Function */
  lua_getglobal(L, "fsm"); /* Get FSM table */
  ret = report(L, lua_pcall(L, 1, 1, 0)); /* call step function */
  lua_pop(L,-1);/* cleanup*/
  return ret;
}

int rfsm_step(lua_State* L) {
  int ret = 0;
  lua_getglobal(L, "rfsm");
  lua_pushstring(L, "step");
  lua_gettable(L, -2); /* Get Step Function */
  lua_getglobal(L, "fsm"); /* Get FSM table */
  ret = report(L, lua_pcall(L, 1, 1, 0)); /* call step function */
  lua_pop(L,-1);/* cleanup*/
  return ret;
}

int rfsm_steps(lua_State* L, int num_steps) {
  int ret = 0;
  lua_getglobal(L, "rfsm");
  lua_pushstring(L, "step");
  lua_gettable(L, -2); /* Get Step Function */
  lua_getglobal(L, "fsm"); /* Get FSM table */
  lua_pushnumber(L, num_steps ); /* Push num_step arg*/ 
  ret = report(L, lua_pcall(L, 2, 1, 0)); /* call step function */
  lua_pop(L,-1);/* cleanup*/
  return ret;
}


int rfsm_sendevent(lua_State* L, char* event) {
  int ret = 0;
  lua_getglobal(L, "rfsm");
  lua_pushstring(L, "send_events");
  lua_gettable(L, -2); /* Get Step Function */
  lua_getglobal(L, "fsm"); /* Get FSM table */
  lua_pushstring(L, event);
  ret = report(L, lua_pcall(L, 2, 1, 0)); /* call step function */
  lua_pop(L,-1);
  return ret;
}

int rfsm_sendevents(lua_State* L, char* events[], unsigned int esize) {
  int ret = 0;
  unsigned int i=0;
  lua_getglobal(L, "rfsm");
  lua_pushstring(L, "send_events");
  lua_gettable(L, -2); /* Get Step Function */
  lua_getglobal(L, "fsm"); /* Get FSM table */
  for(i=0;i<esize;i++) {
    lua_pushstring(L, events[i]);
  }
  ret = report(L, lua_pcall(L, 1+esize, 1, 0)); /* call step function */
  lua_pop(L,-1);
  return ret;
}

int rfsm_init_fsm(lua_State* L, char* filename) {
  int ret = 0;
  lua_getglobal(L, "rfsm");
  lua_pushstring(L, "load");
  lua_gettable(L, -2); /* get Load */
  lua_pushstring(L, filename); /* setup arg filename */
  ret = report(L, lua_pcall(L, 1, 1, 0));
  if(ret!=0)
    return ret;
  lua_pushvalue(L, -2); /* recover rfsm */
  lua_pushstring(L, "init");
  lua_gettable(L, -2); /* get init */
  lua_pushvalue(L, -3); /* previous result */
  ret = report(L, lua_pcall(L, 1, 1, 0));
  if(ret!=0)
    return ret;
  lua_setglobal(L, "fsm");
  lua_pop(L,3);
  return ret;
}