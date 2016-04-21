#include "mediator.h"

// mediator self test

int mediator_test (bool verbose) {
    printf (" * mediator: ");
    if (verbose)
        printf ("\n");
    
    // @selftest
    // Create two mediators
    json_t * config1 = load_config_file("../examples/configs/wasp1.json");
    assert (config1);
    mediator_t *mediator1 = mediator_new(config1);
    assert (mediator1);
    
    json_t * config2 = load_config_file("../examples/configs/wasp2.json");
    assert (config2);
    mediator_t *mediator2 = mediator_new(config2);
    assert (mediator2);
    
    if (verbose) {
        zyre_set_verbose (mediator1->remote);
        zyre_set_verbose (mediator1->local);
        zyre_set_verbose (mediator2->remote);
        zyre_set_verbose (mediator2->local);
    }
    
    // Give them time to interconnect
    zclock_sleep (100);
    if (verbose) {
        zyre_dump (mediator1->remote);
        zyre_dump (mediator1->local);
        zyre_dump (mediator2->remote);
        zyre_dump (mediator2->local);
    }
    
    return 0; 
}

int
main (int argc, char *argv [])
{
    bool verbose;
    if (argc == 2 && streq (argv [1], "-v"))
        verbose = true;
    else
        verbose = false;

    printf ("Running mediator selftests...\n");

    mediator_test (verbose);
    
    printf ("Tests passed OK\n");
    return 0;

}

