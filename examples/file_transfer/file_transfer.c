//  File Transfer model #3
//  
//  In which the client requests each chunk individually, using
//  command pipelining to give us a credit-based flow control.

#include <czmq.h>
#define CHUNK_SIZE  250000
#define PIPELINE    10

static void
client_actor (zsock_t *pipe, void *args)
{
    char* endpoint = (char*)args;
    FILE *file = fopen ("tmp", "w");
    assert (file);
    printf("Endpoint: %s\n", endpoint);
    zsock_t *dealer = zsock_new_dealer(endpoint);
    assert(dealer); 
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
    fclose (file);
    zstr_send (pipe, "OK");
    zpoller_destroy(&poller);
    zsock_destroy(&dealer);
}

//  The server thread waits for a chunk request from a client,
//  reads that chunk and sends it back to the client:

static void
server_actor (zsock_t *pipe, void *args)
{
    char* uri = strdup((char*)args);
    printf("URI: %s\n", uri);
    char* token;
    token = strtok(uri, ":");
    token = strtok(NULL, ":");
    char* filename = strdup(token);
    printf("File: %s\n", filename);
    FILE *file = fopen (filename, "r");
    assert (file);

    zsock_t *router = zsock_new_router ("tcp://*:*");
    //  We have two parts per message so HWM is PIPELINE * 2
    zsocket_set_hwm (zsock_resolve(router), PIPELINE * 2);
    
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    zstr_send(pipe, zsock_endpoint(router));

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
 //   }
    fclose (file);
   zpoller_destroy(&poller);
    zsock_destroy(&router);
}

//  The main task starts the client and server threads; it's easier
//  to test this as a single process with threads, than as multiple
//  processes:

int main (void)
{
    //  Start child threads
    //zctx_t *ctx = zctx_new ();
    zactor_t *server = zactor_new(server_actor, "peerid:./testdata");
    char* endpoint = zstr_recv(server);
    char* token;
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
    zactor_t *client = zactor_new(client_actor, endpoint);
    //  Loop until client tells us it's done
    char *string = zstr_recv (client);
    free (string);
    //  Kill server thread
    zactor_destroy(&server);
    zactor_destroy(&client);
    return 0;
}
