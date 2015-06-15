//  --------------------------------------------------------------------------
//  Zyre is Copyright (c) 2010-2014 iMatix Corporation and Contributors
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  --------------------------------------------------------------------------
#include <jansson.h>
#include <zyre.h>
#define ENCODER_INTERVAL  1000    //  msecs
#define MODEL_URI "http://people.mech.kuleuven.be/~jphilips/json/encoder.json"

static void encoder_actor (zsock_t *pipe, void *args) {
    char** argv = (char**) args;
    char* name = (char*) argv[1];
    char* group = (char*) argv[2];
    zyre_t *node = zyre_new(name);
    zyre_set_header(node,"MODEL", MODEL_URI); 
    if (!node)
 	return;
    zyre_start(node);
    zyre_join(node, group);
    zsock_signal(pipe,0);
    int tick = 0;
    uint64_t encoder_at = zclock_time () + ENCODER_INTERVAL;
    while (!zsys_interrupted) {
	if (zclock_time () >= encoder_at) {
	    zyre_shouts(node, group, "%i\n", tick);
	    tick = tick+1;
            encoder_at = zclock_time () + ENCODER_INTERVAL;
        }	
    }
    // Notify peers that this peer is shutting down. Provide
    // a brief interval to ensure message is emitted.
    zyre_stop(node);
    zclock_sleep(100);

    zyre_destroy (&node);
}

int
main (int argc, char *argv[])
{
    if (argc < 3) {
        puts ("syntax: ./encoder name group");
        exit (0);
    }
    zactor_t *actor = zactor_new (encoder_actor, argv);
    assert (actor);
    
    while (!zsys_interrupted) {
        zclock_sleep(100);
    }

    zactor_destroy (&actor);

    return 0;
}
