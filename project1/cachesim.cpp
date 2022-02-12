#include "cachesim.hpp"

/* global variables for cache config */
uint64_t b1, c1, s1;
uint64_t b2, c2, s2;
uint64_t victim_entries;
insert_policy_t policy2;
bool disable2;

/**
 * Subroutine for initializing the cache simulator. You many add and initialize any global or heap
 * variables as needed.
 */

void sim_setup(sim_config_t *config) {
    /* initialize global cache config values */
    b1 = config->l1_config.b;
    c1 = config->l1_config.c;
    s1 = config->l1_config.s;
    b2 = config->l2_config.b;
    c2 = config->l2_config.c;
    s2 = config->l2_config.s;
    victim_entries = config->victim_cache_entries;
    policy2 = config->l2_config.insert_policy;
    disable2 = config->l2_config.disabled;
    
}
 
/**
 * Subroutine that simulates the cache one trace event at a time.
 */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {

}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 */
void sim_finish(sim_stats_t *stats) {

}
