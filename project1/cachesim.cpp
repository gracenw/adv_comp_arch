#include "cachesim.hpp"

#include <math.h>

static int addr_size = 64;

typedef struct block {
    uint64_t tag;
    bool valid;
    bool dirty;
} block_t;

typedef struct cache {
    uint64_t num_sets;
    uint64_t num_offset_bits;
    uint64_t num_index_bits;
    uint64_t num_tag_bits;
    uint64_t cache_size;
    uint64_t block_size;
    uint64_t num_ways;
    uint64_t** lru_stack;
    block_t** blocks;
    write_strat_t write_policy;
    insert_policy_t insert_policy;
    bool disabled;
} cache_t;

cache_t l1_cache;
cache_t l2_cache;
cache_t vi_cache;

/**
 * Subroutine for initializing the cache simulator. You may add and initialize any global or heap
 * variables as needed.
 */

void sim_setup(sim_config_t *config) {
    /* initialize l1 global cache config values */
    l1_cache.block_size      = pow(2, config->l1_config.b);
    l1_cache.num_ways        = pow(2, config->l1_config.s);
    l1_cache.cache_size      = pow(2, config->l1_config.c);
    l1_cache.num_sets        = l1_cache.cache_size / l1_cache.block_size / l1_cache.num_ways;
    l1_cache.num_offset_bits = config->l1_config.b;
    l1_cache.num_index_bits  = config->l1_config.c - config->l1_config.s - config->l1_config.b;
    l1_cache.num_tag_bits    = addr_size - (l1_cache.num_index_bits + l1_cache.num_offset_bits);
    l1_cache.write_policy    = WRITE_STRAT_WBWA;
    l1_cache.insert_policy   = INSERT_POLICY_MIP;
    l1_cache.disabled        = false;
    l1_cache.lru_stack       = (uint64_t**) malloc(sizeof(uint64_t) * (l1_cache.num_sets * l1_cache.num_ways));
    l1_cache.blocks          = (block_t**) malloc(sizeof(block_t) * (l1_cache.num_sets * l1_cache.num_ways));

    /* initialize l2 global cache config values */
    l2_cache.block_size      = pow(2, config->l2_config.b);
    l2_cache.num_ways        = pow(2, config->l2_config.s);
    l2_cache.cache_size      = pow(2, config->l2_config.c);
    l2_cache.num_sets        = l2_cache.cache_size / l2_cache.block_size / l2_cache.num_ways;
    l2_cache.num_offset_bits = config->l2_config.b;
    l2_cache.num_index_bits  = config->l2_config.c - config->l2_config.s - config->l2_config.b;
    l2_cache.num_tag_bits    = addr_size - (l2_cache.num_index_bits + l2_cache.num_offset_bits);
    l2_cache.write_policy    = WRITE_STRAT_WTWNA;
    l2_cache.insert_policy   = config->l2_config.insert_policy;
    l2_cache.disabled        = config->l2_config.disabled;
    l1_cache.lru_stack       = (uint64_t**) malloc(sizeof(uint64_t) * (l2_cache.num_sets * l2_cache.num_ways));
    l1_cache.blocks          = (block_t**) malloc(sizeof(block_t) * (l2_cache.num_sets * l2_cache.num_ways));

    /* initialize victim global cache config values */
    vi_cache.block_size      = pow(2, config->l1_config.b);
    vi_cache.num_ways        = config->victim_cache_entries;
    vi_cache.cache_size      = vi_cache.block_size * vi_cache.num_ways;
    vi_cache.num_sets        = 1;
    vi_cache.num_offset_bits = config->l1_config.b;
    vi_cache.num_index_bits  = 0;
    vi_cache.num_tag_bits    = addr_size - vi_cache.num_offset_bits;
    // vi_cache.write_policy    = WRITE_STRAT_WBWA;
    vi_cache.insert_policy   = INSERT_POLICY_MIP;
    vi_cache.disabled        = !(config->victim_cache_entries);
    l1_cache.lru_stack       = (uint64_t**) malloc(sizeof(uint64_t) * (vi_cache.num_sets * vi_cache.num_ways));
    l1_cache.blocks          = (block_t**) malloc(sizeof(block_t) * (vi_cache.num_sets * vi_cache.num_ways));
}
 
/**
 * Subroutine that simulates the cache one trace event at a time.
 */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    /* get tag, index, offset */
    int offset_start, index_start, tag_start;
    uint64_t tag, index, offset;

    bool hit = false;

    offset_start = 0;
    index_start = l1_cache.num_offset_bits;
    tag_start = l1_cache.num_index_bits + index_start;
    tag = (addr >> tag_start) & ((1 << l1_cache.num_tag_bits) - 1);
    index = (addr >> index_start) & ((1 << l1_cache.num_index_bits) - 1);
    offset = (addr >> offset_start) & ((1 << l1_cache.num_offset_bits) - 1);

    /* search l1 cache for tag */
    for (int i = 0; i < l1_cache.num_ways; i++) {
        if (l1_cache.blocks[index][i].tag == tag) {
            hit = true;
        }
    }

    /* l1 cache hit */
    if (hit) {
        // fill in
        return;
    }
    
    /* check if victim cache is enabled */
    if (!vi_cache.disabled) {
        /* update tag and offset*/
        tag_start = vi_cache.num_offset_bits;
        tag = (addr >> tag_start) & ((1 << vi_cache.num_tag_bits) - 1);
        offset = (addr >> offset_start) & ((1 << vi_cache.num_offset_bits) - 1);

        /* search victim cache for tag */
        for (int i = 0; i < vi_cache.num_ways; i++) {
            if (vi_cache.blocks[0][i].tag == tag) {
                hit = true;
            }
        }

        /* victim cache hit */
        if (hit) {
            // fill in
            return;
        }
    }


    /* check if l2 cache is enabled */
    if (!l2_cache.disabled) {
        /* update tag, offset, and index */
        index_start = l2_cache.num_offset_bits;
        tag_start = l2_cache.num_index_bits + index_start;
        tag = (addr >> tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
        index = (addr >> index_start) & ((1 << l2_cache.num_index_bits) - 1);
        offset = (addr >> offset_start) & ((1 << l2_cache.num_offset_bits) - 1);

        /* search l2 cache for tag */
        for (int i = 0; i < l2_cache.num_ways; i++) {
            if (l2_cache.blocks[index][i].tag == tag) {
                hit = true;
            }
        }

        /* l2 cache hit */
        if (hit) {
            // fill in
            return;
        }
    }

    /* total cache miss */
    // fill in
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 */
void sim_finish(sim_stats_t *stats) {
    /* calculate stats */


    /* clean up memory */
    free(l1_cache.blocks);
    free(l1_cache.lru_stack);
    free(l2_cache.blocks);
    free(l2_cache.lru_stack);
    free(vi_cache.blocks);
    free(vi_cache.lru_stack);
}

