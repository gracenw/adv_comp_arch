#include "cachesim.hpp"

/* global cache variables */
cache_t l1_cache;
cache_t l2_cache;
cache_t vi_cache;

void set_mru(std::vector<uint64_t>& stack, uint64_t tag) {
	/* temporary array to hold values */
    std::vector<uint64_t> temp(stack.size() - 1);

	int j = 0;
	for (int i = 0; i < stack.size(); i++) {
        /* special case for lru eviction - won't try to save the lru in the temp array */
        if (i == j && i == (stack.size() - 1)) {
            break;
        }

		/* only store values that are not MRU in temp array */
		if (stack[i] != tag) {
			temp[j] = stack[i];
			j++;
		}
	}
	
    /* set n-block to MRU position */
	stack[0] = tag;

	/* copy over previous indices, shifted down */
	for (int k = 1; k < stack.size(); k++) {
		stack[k] = temp[k-1];
	}
}

void set_lru(std::vector<uint64_t>& stack, uint64_t tag) {
	/* temporary array to hold values */
    std::vector<uint64_t> temp(stack.size() - 1);

	int j = 0;
	for (int i = 0; i < stack.size(); i++) {
        /* special case for lru eviction - won't try to save the lru in the temp array */
        if (i == j && i == (stack.size() - 1)) {
            break;
        }

		/* only store values that are not LRU in temp array */
		if (stack[i] != tag) {
			temp[j] = stack[i];
			j++;
		}
	}
	
    /* set n-block to LRU position */
	stack[stack.size() - 1] = tag;

	/* copy over previous indices, shifted down */
	for (int k = 0; k < stack.size() - 1; k++) {
		stack[k] = temp[k];
	}
}

void popoff_stack(std::vector<uint64_t>& stack, uint64_t tag) {
    /* remove hit block from victim lru stack */
    for (int i = 0; i < stack.size(); i++) {
        if (stack[i] == tag) {
            stack[i] = NULL;
            break;
        }
    }

    /* temporary array to hold values */
    std::vector<uint64_t> temp(stack.size());

    /* move all null spots to bottom of stack */
    int j = 0;
    for (int i = 0; i < stack.size(); i++) {
        if (stack[i] != NULL) {
            temp[j] = stack[i];
            j++;
        }
    }
}

uint64_t get_lru(std::vector<uint64_t>& stack) {
    /* loop until end of valid tags and return index */
    int i;
    for (i = 0; i < stack.size(); i++) {
        if (stack[i] == NULL) break;
    }
    return stack[i - 1];
}

int available(std::vector<block_t*>& set) {
    /* return index on first open block */
    for (int i = 0; i < set.size(); i++) {
        if (!set[i]->valid) return i;
    }
    /* return -1 for no open blocks */
    return -1;
}

/* subroutine for initializing the cache simulator */
void sim_setup(sim_config_t *config) {
    /* initialize l1 global cache config values */
    l1_cache.block_size      = pow(2, config->l1_config.b);
    l1_cache.num_ways        = pow(2, config->l1_config.s);
    l1_cache.cache_size      = pow(2, config->l1_config.c);
    l1_cache.num_sets        = l1_cache.cache_size / l1_cache.block_size / l1_cache.num_ways;
    l1_cache.num_offset_bits = config->l1_config.b;
    l1_cache.num_index_bits  = config->l1_config.c - config->l1_config.s - config->l1_config.b;
    l1_cache.num_tag_bits    = addr_size - (l1_cache.num_index_bits + l1_cache.num_offset_bits);

    /* reserve space for cache vectors */
    l1_cache.sets.reserve(l1_cache.num_sets);
    for (int i = 0; i < l1_cache.num_sets; i++) {
        l1_cache.sets[i]->blocks.reserve(l1_cache.num_ways);
        l1_cache.sets[i]->lru_stack.reserve(l1_cache.num_ways);
    }

    /* initialize l2 global cache config values */
    l2_cache.disabled = config->l2_config.disabled;

    if (!l2_cache.disabled) {
        l2_cache.block_size      = pow(2, config->l2_config.b);
        l2_cache.num_ways        = pow(2, config->l2_config.s);
        l2_cache.cache_size      = pow(2, config->l2_config.c);
        l2_cache.num_sets        = l2_cache.cache_size / l2_cache.block_size / l2_cache.num_ways;
        l2_cache.num_offset_bits = config->l2_config.b;
        l2_cache.num_index_bits  = config->l2_config.c - config->l2_config.s - config->l2_config.b;
        l2_cache.num_tag_bits    = addr_size - (l2_cache.num_index_bits + l2_cache.num_offset_bits);
        l2_cache.insert_policy   = config->l2_config.insert_policy;

        /* reserve space for cache vectors */
        l2_cache.sets.reserve(l2_cache.num_sets);
        for (int i = 0; i < l2_cache.num_sets; i++) {
            l2_cache.sets[i]->blocks.reserve(l2_cache.num_ways);
            l2_cache.sets[i]->lru_stack.reserve(l2_cache.num_ways);
        }
    }

    /* initialize victim global cache config values */
    vi_cache.disabled = !(config->victim_cache_entries);
    
    if (!vi_cache.disabled) {
        vi_cache.block_size         = pow(2, config->l1_config.b);
        vi_cache.num_ways           = config->victim_cache_entries;
        vi_cache.cache_size         = vi_cache.block_size * vi_cache.num_ways;
        vi_cache.num_sets           = 1;
        vi_cache.num_offset_bits    = config->l1_config.b;
        vi_cache.num_index_bits     = 0;
        vi_cache.num_tag_bits       = addr_size - vi_cache.num_offset_bits;

        /* reserve space for cache vectors */
        vi_cache.sets.reserve(1);
        vi_cache.sets[0]->blocks.reserve(vi_cache.num_ways);
        vi_cache.sets[0]->lru_stack.reserve(vi_cache.num_ways);
    }
}

/* subroutine that simulates the cache one trace event at a time */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    /* get tag, index, offset */
    int offset_start = 0;
    int l1_index_start = l1_cache.num_offset_bits;
    int l1_tag_start = l1_cache.num_index_bits + l1_index_start;
    uint64_t l1_tag = (addr >> l1_tag_start) & ((1 << l1_cache.num_tag_bits) - 1);
    uint64_t l1_index = (addr >> l1_index_start) & ((1 << l1_cache.num_index_bits) - 1);

    /* increment l1 accesses */
    stats->accesses_l1++;

    /* search l1 cache for tag */
    bool hit = false;
    int block;
    for (block = 0; block < l1_cache.num_ways; block++) {
        if (l1_cache.sets[l1_index]->blocks[block]->tag == l1_tag && l1_cache.sets[l1_index]->blocks[block]->valid) {
            hit = true;
            break;
        }
    }

    /* l1 cache hit */
    if (hit) {
        /* increment hits */
        stats->hits_l1++;

        /* determine if read or write */
        if (rw == WRITE) {
            /* set dirty bit */
            l1_cache.sets[l1_index]->blocks[block]->dirty = true;

            /* increment writes */
            stats->writes++;
        }
        else {
            /* increment reads */
            stats->reads++;
        }

        /* set hit block to MRU */
        set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);

        return;
    }

    /* function did not return, increment l1 misses */
    stats->misses_l1++;
    
    /* check if victim cache is enabled */
    int vi_tag_start;
    uint64_t vi_tag;
    if (!vi_cache.disabled) {
        /* update tag and offset*/
        vi_tag_start = vi_cache.num_offset_bits;
        vi_tag = (addr >> vi_tag_start) & ((1 << vi_cache.num_tag_bits) - 1);

        /* search victim cache for tag */
        for (block = 0; block < vi_cache.num_ways; block++) {
            if (vi_cache.sets[0]->blocks[block]->tag == vi_tag && vi_cache.sets[0]->blocks[block]->valid) {
                hit = true;
                break;
            }
        }

        /* victim cache hit */
        if (hit) {
            /* increment hits */
            stats->hits_victim_cache++;

            /* determine if read or write */
            if (rw == WRITE) {
                /* set dirty bit */
                vi_cache.sets[0]->blocks[block]->dirty = true;

                /* increment writes */
                stats->writes++;
            }
            else {
                /* increment reads */
                stats->reads++;
            }

            /* see if set in l1 has empty spots */
            int open_block = available(l1_cache.sets[l1_index]->blocks);
            if (open_block < 0) {
                /* no open spots in l1 set, must evict - get the lru for the hit block's set in l1 */
                uint64_t l1_lru_tag = get_lru(l1_cache.sets[l1_index]->lru_stack);

                /* swap actual blocks in victim and l1 */
                for (int i = 0; i < l1_cache.num_ways; i++) {
                    if (l1_cache.sets[l1_index]->blocks[i]->tag == l1_lru_tag && l1_cache.sets[l1_index]->blocks[i]->valid) {
                        /* exchange block info */
                        int temp_hit_dirty = vi_cache.sets[0]->blocks[block]->dirty;

                        vi_cache.sets[0]->blocks[block]->dirty = l1_cache.sets[l1_index]->blocks[i]->dirty;
                        vi_cache.sets[0]->blocks[block]->tag = (l1_cache.sets[l1_index]->blocks[i]->addr >> vi_tag_start) & ((1 << vi_cache.num_tag_bits) - 1);
                        vi_cache.sets[0]->blocks[block]->valid = true;
                        vi_cache.sets[0]->blocks[block]->addr = l1_cache.sets[l1_index]->blocks[i]->addr;

                        l1_cache.sets[l1_index]->blocks[i]->dirty = temp_hit_dirty;
                        l1_cache.sets[l1_index]->blocks[i]->valid = true;
                        l1_cache.sets[l1_index]->blocks[i]->tag = l1_tag;
                        l1_cache.sets[l1_index]->blocks[i]->addr = addr;

                        break;
                    }
                }

                /* swap tags in victim and l1 lru stacks */
                set_mru(vi_cache.sets[0]->lru_stack, vi_cache.sets[0]->blocks[block]->tag);
                set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);

                return;
            }
            else {
                /* open spot available, save hit block in l1 */
                l1_cache.sets[l1_index]->blocks[open_block]->tag = l1_tag;
                l1_cache.sets[l1_index]->blocks[open_block]->addr = addr;
                l1_cache.sets[l1_index]->blocks[open_block]->dirty = vi_cache.sets[0]->blocks[block]->dirty;
                l1_cache.sets[l1_index]->blocks[open_block]->valid = true;

                /* remove hit block from victim lru stack */
                popoff_stack(vi_cache.sets[0]->lru_stack, vi_tag);

                /* remove hit block from victim block list */
                vi_cache.sets[0]->blocks[block]->valid = 0;

                /* set incoming hit block to mru in l1 */
                set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);
            }
        }
    }

    /* victim cache disabled or function did not return, increment victim misses */
    stats->misses_victim_cache++;

    /* check if l2 cache is enabled */
    int l2_index_start, l2_tag_start;
    uint64_t l2_tag, l2_index;
    if (!l2_cache.disabled) {
        /* increment r/w request stats */
        if (rw == WRITE) {
            stats->writes_l2++;
        }
        else {
            stats->reads_l2++;
        }

        /* update tag, offset, and index */
        l2_index_start = l2_cache.num_offset_bits;
        l2_tag_start = l2_cache.num_index_bits + l2_index_start;
        l2_tag = (addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
        l2_index = (addr >> l2_index_start) & ((1 << l2_cache.num_index_bits) - 1);

        /* search l2 cache for tag */
        int block;
        for (block = 0; block < l2_cache.num_ways; block++) {
            if (l2_cache.sets[l2_index]->blocks[block]->tag == l2_tag && l2_cache.sets[l2_index]->blocks[block]->valid) {
                hit = true;
                break;
            }
        }

        /* l2 cache hit */
        if (hit) {
            /* determine if read or write */
            if (rw == WRITE) {
                /* increment writes */
                stats->writes++;
            }
            else {
                /* increment reads */
                stats->reads++;

                /* increment l2 read hits */
                stats->read_hits_l2++;
            }

            /* pop off hit block from l2 lru stack, set as new l1 mru */
            popoff_stack(l2_cache.sets[l2_index]->lru_stack, l2_tag);
            
            /* remove hit block from l2 */
            l2_cache.sets[l2_index]->blocks[block]->valid = false;

            /* check if there are open spots in l1 set for l2 hit block */
            int open_block = available(l1_cache.sets[l1_index]->blocks);
            block_t l1_evicted_block;
            if (open_block < 0) {
                /* no spots available, get l1 lru tag */
                uint64_t l1_lru_tag = get_lru(l1_cache.sets[l1_index]->lru_stack);

                /* get index of l1 lru in blocks */
                int l1_lru;
                for (l1_lru = 0; l1_lru < l1_cache.num_ways; l1_lru++) {
                    if (l1_cache.sets[l1_index]->blocks[l1_lru]->tag == l1_lru_tag) break;
                }

                /* bring in l2 hit, evict l1 lru to victim (if enabled) */
                l1_evicted_block.addr = l1_cache.sets[l1_index]->blocks[l1_lru]->addr;
                l1_evicted_block.dirty = l1_cache.sets[l1_index]->blocks[l1_lru]->dirty;

                l1_cache.sets[l1_index]->blocks[l1_lru]->addr = addr;
                l1_cache.sets[l1_index]->blocks[l1_lru]->tag = l1_tag;
                l1_cache.sets[l1_index]->blocks[l1_lru]->valid = true;
                l1_cache.sets[l1_index]->blocks[l1_lru]->dirty = false;

                popoff_stack(l1_cache.sets[l1_index]->lru_stack, l1_lru_tag);
                set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);
            }
            else {
                /* open spot in hit block's l1 set, bring in block */
                l1_cache.sets[l1_index]->blocks[open_block]->addr = addr;
                l1_cache.sets[l1_index]->blocks[open_block]->tag = l1_tag;
                l1_cache.sets[l1_index]->blocks[open_block]->valid = true;
                l1_cache.sets[l1_index]->blocks[open_block]->dirty = false;

                set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);

                return;
            }            

            /* now save l1 victim to victim cache - if enabled and there is a victim */
            block_t vi_evicted_block;
            if (!vi_cache.disabled) {
                /* check for open blocks in victim cache */
                int open_block = available(vi_cache.sets[0]->blocks);

                if (open_block < 0) {
                    /* no open blocks, need to evict victim lru */
                    uint64_t vi_lru_tag = get_lru(vi_cache.sets[0]->lru_stack);

                    /* get index of victim lru block */
                    int vi_lru;
                    for (vi_lru = 0; vi_lru < vi_cache.num_ways; vi_lru++) {
                        if (vi_cache.sets[0]->blocks[vi_lru]->tag == vi_lru_tag) break;
                    }

                    /* save evicted victim lru to temp block */
                    vi_evicted_block.addr = vi_cache.sets[0]->blocks[vi_lru]->addr;
                    vi_evicted_block.dirty = vi_cache.sets[0]->blocks[vi_lru]->dirty;

                    /* update victim cache block with evicted l1 block info */
                    vi_cache.sets[0]->blocks[vi_lru]->addr = l1_evicted_block.addr;
                    vi_cache.sets[0]->blocks[vi_lru]->tag = (l1_evicted_block.addr >> vi_tag_start) & ((1 << vi_cache.num_tag_bits) - 1);
                    vi_cache.sets[0]->blocks[vi_lru]->dirty = l1_evicted_block.dirty;
                    vi_cache.sets[0]->blocks[vi_lru]->valid = true;

                    /* remove victim cache lru from lru stack */
                    popoff_stack(vi_cache.sets[0]->lru_stack, vi_lru_tag);

                    /* set l1 victim tag, now in victim cache, to mru */
                    set_mru(vi_cache.sets[0]->lru_stack, vi_cache.sets[0]->blocks[vi_lru]->tag);
                }
                else {
                    /* open spot in victim cache, bring in l1 victim */
                    vi_cache.sets[0]->blocks[open_block]->addr = l1_evicted_block.addr;
                    vi_cache.sets[0]->blocks[open_block]->tag = (l1_evicted_block.addr >> vi_tag_start) & ((1 << vi_cache.num_tag_bits) - 1);;
                    vi_cache.sets[0]->blocks[open_block]->valid = true;
                    vi_cache.sets[0]->blocks[open_block]->dirty = l1_evicted_block.dirty;

                    /* set newly filled block to mru */
                    set_mru(vi_cache.sets[0]->lru_stack, vi_cache.sets[0]->blocks[open_block]->tag);

                    return;
                }
            }

            /* save either l1 lru or victim lru to l2 */
            uint64_t victim_index;
            if (vi_evicted_block.addr == NULL) {
                /* current incoming block is from l1 */
                victim_index = (l1_evicted_block.addr >> l2_index_start) & ((1 << l2_cache.num_index_bits) - 1);
            }
            else {
                /* current incoming block is from the victim cache */
                victim_index = (vi_evicted_block.addr >> l2_index_start) & ((1 << l2_cache.num_index_bits) - 1);
            }
            
            /* check for open blocks in l2 cache */
            open_block = available(l2_cache.sets[victim_index]->blocks);

            if (open_block < 0) {
                /* no open blocks, need to evict l2 block */
                uint64_t l2_lru_tag = get_lru(l2_cache.sets[victim_index]->lru_stack);

                /* get index of victim lru block */
                int l2_lru;
                for (l2_lru = 0; l2_lru < l2_cache.num_ways; l2_lru++) {
                    if (l2_cache.sets[0]->blocks[l2_lru]->tag == l2_lru_tag) break;
                }

                /* update l2 cache block with evicted l1/victim block info */
                if (vi_evicted_block.addr == NULL) {
                    l2_cache.sets[victim_index]->blocks[l2_lru]->addr = l1_evicted_block.addr;
                    l2_cache.sets[victim_index]->blocks[l2_lru]->tag = (l1_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
                    l2_cache.sets[victim_index]->blocks[l2_lru]->dirty = false;
                    l2_cache.sets[victim_index]->blocks[l2_lru]->valid = true;
                }
                else {
                    l2_cache.sets[victim_index]->blocks[l2_lru]->addr = vi_evicted_block.addr;
                    l2_cache.sets[victim_index]->blocks[l2_lru]->tag = (vi_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
                    l2_cache.sets[victim_index]->blocks[l2_lru]->dirty = false;
                    l2_cache.sets[victim_index]->blocks[l2_lru]->valid = true;
                }

                /* remove l2 cache lru from lru stack */
                popoff_stack(l2_cache.sets[victim_index]->lru_stack, l2_lru_tag);

                /* set l1/victim lru tag, now in l2 cache, to mru OR lru depending on insertion policy */
                if (l2_cache.insert_policy == INSERT_POLICY_MIP) {
                    set_mru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[l2_lru]->tag);
                }
                else {
                    set_lru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[l2_lru]->tag);
                }
            }
            else {
                /* open spot in l2 cache, bring in l1/victim lru */
                if (vi_evicted_block.addr == NULL) {
                    l2_cache.sets[victim_index]->blocks[open_block]->addr = l1_evicted_block.addr;
                    l2_cache.sets[victim_index]->blocks[open_block]->tag = (l1_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
                    l2_cache.sets[victim_index]->blocks[open_block]->dirty = false;
                    l2_cache.sets[victim_index]->blocks[open_block]->valid = true;
                }
                else {
                    l2_cache.sets[victim_index]->blocks[open_block]->addr = vi_evicted_block.addr;
                    l2_cache.sets[victim_index]->blocks[open_block]->tag = (vi_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
                    l2_cache.sets[victim_index]->blocks[open_block]->dirty = false;
                    l2_cache.sets[victim_index]->blocks[open_block]->valid = true;
                }

                /* set newly filled block to mru OR lru depending on insertion policy */
                if (l2_cache.insert_policy == INSERT_POLICY_MIP) {
                    set_mru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[open_block]->tag);
                }
                else {
                    set_lru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[open_block]->tag);
                }

                return;
            }

            /* if an l2 block is evicted, it would be saved to DRAM here */
            return;
        }
    }

    /* increment l2 read miss if no hit & read operation */
    if (rw == READ) {
        stats->read_misses_l2++;
    }

    /* TOTAL CACHE MISS - now bring block in from memory, and cascade down with any victim blocks */

    /* see if there are any spots in l1 */
    int open_block = available(l1_cache.sets[l1_index]->blocks);
    block_t l1_evicted_block;
    if (open_block < 0) {
        /* no spots available, get l1 lru tag */
        uint64_t l1_lru_tag = get_lru(l1_cache.sets[l1_index]->lru_stack);

        /* get index of l1 lru in blocks */
        int l1_lru;
        for (l1_lru = 0; l1_lru < l1_cache.num_ways; l1_lru++) {
            if (l1_cache.sets[l1_index]->blocks[l1_lru]->tag == l1_lru_tag) break;
        }

        /* bring in DRAM hit, evict l1 lru to victim (if enabled) */
        l1_evicted_block.addr = l1_cache.sets[l1_index]->blocks[l1_lru]->addr;
        l1_evicted_block.dirty = l1_cache.sets[l1_index]->blocks[l1_lru]->dirty;

        l1_cache.sets[l1_index]->blocks[l1_lru]->addr = addr;
        l1_cache.sets[l1_index]->blocks[l1_lru]->tag = l1_tag;
        l1_cache.sets[l1_index]->blocks[l1_lru]->valid = true;
        l1_cache.sets[l1_index]->blocks[l1_lru]->dirty = false;

        popoff_stack(l1_cache.sets[l1_index]->lru_stack, l1_lru_tag);
        set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);
    }
    else {
        /* save block from memory into open block */
        l1_cache.sets[l1_index]->blocks[open_block]->tag = l1_tag;
        l1_cache.sets[l1_index]->blocks[open_block]->addr = addr;
        l1_cache.sets[l1_index]->blocks[open_block]->valid = true;
        l1_cache.sets[l1_index]->blocks[open_block]->dirty = false;

        /* set incoming block as the mru */
        set_mru(l1_cache.sets[l1_index]->lru_stack, l1_tag);

        return;
    }

    /* now check victim cache for open spots - if there was an eviction in previous stage */
    block_t vi_evicted_block;
    if (!vi_cache.disabled) {
        /* check for open blocks in victim cache */
        int open_block = available(vi_cache.sets[0]->blocks);

        if (open_block < 0) {
            /* no open blocks, need to evict victim lru */
            uint64_t vi_lru_tag = get_lru(vi_cache.sets[0]->lru_stack);

            /* get index of victim lru block */
            int vi_lru;
            for (vi_lru = 0; vi_lru < vi_cache.num_ways; vi_lru++) {
                if (vi_cache.sets[0]->blocks[vi_lru]->tag == vi_lru_tag) break;
            }

            /* save evicted victim lru to temp block */
            vi_evicted_block.addr = vi_cache.sets[0]->blocks[vi_lru]->addr;
            vi_evicted_block.dirty = vi_cache.sets[0]->blocks[vi_lru]->dirty;

            /* update victim cache block with evicted l1 block info */
            vi_cache.sets[0]->blocks[vi_lru]->addr = l1_evicted_block.addr;
            vi_cache.sets[0]->blocks[vi_lru]->tag = (l1_evicted_block.addr >> vi_tag_start) & ((1 << vi_cache.num_tag_bits) - 1);
            vi_cache.sets[0]->blocks[vi_lru]->dirty = l1_evicted_block.dirty;
            vi_cache.sets[0]->blocks[vi_lru]->valid = true;

            /* remove victim cache lru from lru stack */
            popoff_stack(vi_cache.sets[0]->lru_stack, vi_lru_tag);

            /* set l1 victim tag, now in victim cache, to mru */
            set_mru(vi_cache.sets[0]->lru_stack, vi_cache.sets[0]->blocks[vi_lru]->tag);
        }
        else {
            /* open spot in victim cache, bring in l1 victim */
            vi_cache.sets[0]->blocks[open_block]->addr = l1_evicted_block.addr;
            vi_cache.sets[0]->blocks[open_block]->tag = (l1_evicted_block.addr >> vi_tag_start) & ((1 << vi_cache.num_tag_bits) - 1);;
            vi_cache.sets[0]->blocks[open_block]->valid = true;
            vi_cache.sets[0]->blocks[open_block]->dirty = l1_evicted_block.dirty;

            /* set newly filled block to mru */
            set_mru(vi_cache.sets[0]->lru_stack, vi_cache.sets[0]->blocks[open_block]->tag);

            return;
        }
    }

    /* finally - save evicted block to l2 if needed and enabled; otherwise block just goes back to DRAM */
    uint64_t victim_index;
    if (vi_evicted_block.addr == NULL) {
        /* current incoming block is from l1 */
        victim_index = (l1_evicted_block.addr >> l2_index_start) & ((1 << l2_cache.num_index_bits) - 1);
    }
    else {
        /* current incoming block is from the victim cache */
        victim_index = (vi_evicted_block.addr >> l2_index_start) & ((1 << l2_cache.num_index_bits) - 1);
    }
    
    /* check for open blocks in l2 cache */
    open_block = available(l2_cache.sets[victim_index]->blocks);

    if (open_block < 0) {
        /* no open blocks, need to evict l2 block */
        uint64_t l2_lru_tag = get_lru(l2_cache.sets[victim_index]->lru_stack);

        /* get index of victim lru block */
        int l2_lru;
        for (l2_lru = 0; l2_lru < l2_cache.num_ways; l2_lru++) {
            if (l2_cache.sets[0]->blocks[l2_lru]->tag == l2_lru_tag) break;
        }

        /* update l2 cache block with evicted l1/victim block info */
        if (vi_evicted_block.addr == NULL) {
            l2_cache.sets[victim_index]->blocks[l2_lru]->addr = l1_evicted_block.addr;
            l2_cache.sets[victim_index]->blocks[l2_lru]->tag = (l1_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
            l2_cache.sets[victim_index]->blocks[l2_lru]->dirty = false;
            l2_cache.sets[victim_index]->blocks[l2_lru]->valid = true;
        }
        else {
            l2_cache.sets[victim_index]->blocks[l2_lru]->addr = vi_evicted_block.addr;
            l2_cache.sets[victim_index]->blocks[l2_lru]->tag = (vi_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
            l2_cache.sets[victim_index]->blocks[l2_lru]->dirty = false;
            l2_cache.sets[victim_index]->blocks[l2_lru]->valid = true;
        }

        /* remove l2 cache lru from lru stack */
        popoff_stack(l2_cache.sets[victim_index]->lru_stack, l2_lru_tag);

        /* set l1/victim lru tag, now in l2 cache, to mru OR lru depending on insertion policy */
        if (l2_cache.insert_policy == INSERT_POLICY_MIP) {
            set_mru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[l2_lru]->tag);
        }
        else {
            set_lru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[l2_lru]->tag);
        }
    }
    else {
        /* open spot in l2 cache, bring in l1/victim lru */
        if (vi_evicted_block.addr == NULL) {
            l2_cache.sets[victim_index]->blocks[open_block]->addr = l1_evicted_block.addr;
            l2_cache.sets[victim_index]->blocks[open_block]->tag = (l1_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
            l2_cache.sets[victim_index]->blocks[open_block]->dirty = false;
            l2_cache.sets[victim_index]->blocks[open_block]->valid = true;
        }
        else {
            l2_cache.sets[victim_index]->blocks[open_block]->addr = vi_evicted_block.addr;
            l2_cache.sets[victim_index]->blocks[open_block]->tag = (vi_evicted_block.addr >> l2_tag_start) & ((1 << l2_cache.num_tag_bits) - 1);
            l2_cache.sets[victim_index]->blocks[open_block]->dirty = false;
            l2_cache.sets[victim_index]->blocks[open_block]->valid = true;
        }

        /* set newly filled block to mru OR lru depending on insertion policy */
        if (l2_cache.insert_policy == INSERT_POLICY_MIP) {
            set_mru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[open_block]->tag);
        }
        else {
            set_lru(l2_cache.sets[victim_index]->lru_stack, l2_cache.sets[victim_index]->blocks[open_block]->tag);
        }

        return;
    }
}

/* subroutine for calculating overall statistics such as miss rate or average access time */
void sim_finish(sim_stats_t *stats) {
    /* calculate stats */
    // stats->avg_access_time_l1 = ;
    // stats->hit_ratio_l1 = stats->hits_l1 / stats->accesses_l1;
    // stats->miss_ratio_l1 = stats->misses_l1 / stats->accesses_l1;

    // stats->hit_ratio_victim_cache = ;
    // stats->miss_ratio_victim_cache = ;

    // stats->avg_access_time_l2 = ;
    // stats->read_hit_ratio_l2 = ;
    // stats->read_miss_ratio_l2 = ;
}

