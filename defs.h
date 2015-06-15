#ifndef PROXY_DEFS
#define PROXY_DEFS "Definitions"

// Dependencies
#include "rfsmbinding.h"
#include <jansson.h>
#include <time.h>
#include <zyre.h>

#define TIMEOUT 1000
#include <curl/curl.h>
 
struct MemoryStruct {
  char *memory;
  size_t size;
}; 
 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}
 
 
char* read_url(char* url)
{
  CURL *curl_handle;
  CURLcode res;
 
  struct MemoryStruct chunk;
 
  chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
  chunk.size = 0;    /* no data at this point */ 
 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();
 
  /* specify URL to get */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, url);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
 
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
 
  /* get it! */ 
  res = curl_easy_perform(curl_handle);
  /* check for errors */ 
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
	return NULL;
  }
  else {
    printf("%lu bytes retrieved\n", (long)chunk.size);
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
    return chunk.memory;
  }
 

} 


// Global lua state
lua_State *L;

typedef struct {
    char* metamodel_id;
    char* model_id;
    char* event_id;
    char* timestamp;
} event_t;

static char* generate_json(event_t* e) {
  json_t *root = json_array();
  json_t *event = json_object();
  json_t *event_data = json_object();
  json_object_set_new( event, "timestamp", json_string(e->timestamp));
  json_object_set_new( event, "event", event_data);
  json_object_set_new( event_data, "metamodel_id", json_string(e->metamodel_id));
  json_object_set_new( event_data, "model_id", json_string(e->model_id));
  json_object_set_new( event_data, "event_id", json_string(e->event_id));
  json_array_append( root, event );
  
  char* ret_strings = json_dumps( root, 0 );
  json_decref(root);
  return ret_strings;
}

typedef struct {
    char *name;
    char *group;
    char *model_uri;
    zyre_t *node; // pointer to Zyre node
    zsock_t *pipe; // main loop socket
    zpoller_t *com; // poller to check communication channels
    zlist_t *input_events; // list of input events
    zlist_t *output_events; // list of output events
    char *input; // latest received input
    char *userinput; // latest user input
    bool deleted;
    bool configured_resources;
} resource_t;

#endif
