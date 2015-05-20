#include "../rfsmbinding/rfsmbinding.h"

void usage() {
  printf("fsmloader usage:\n\t fsmloader <fsm_model> <node name>\n\t\t-fsm uri to json model\n\t\t-optional app name\n");
}

int main(int argc, char** argv)
{
  lua_State *L;
  L = luaL_newstate();                        /* Create Lua state variable */
  luaL_openlibs(L);  

  char *fsm;
  char *name;
  if(argc==2) {
    fsm = argv[1];
    lua_pushstring(L, fsm);
    lua_setglobal(L, "fsm_uri");
  }
  else if(argc==3) {
    fsm = argv[1];
    name = argv[2];
    lua_pushstring(L, fsm);
    lua_setglobal(L, "fsm_uri");
    lua_pushstring(L, name);
    lua_setglobal(L, "name");
  }
  else {
    usage();
    goto out;
  }
  
  if (luaL_loadfile(L, "scripts/gen-rfsm.lua")) {
    printf("Error load gen fsm script..check path!\n");
    goto out;
  }
  lua_pcall(L,0,0,0);

  rfsm_run(L);
  rfsm_sendevent(L, "e_activate");
  rfsm_run(L);
  
out:    
  lua_close(L);
  return 0;
}
