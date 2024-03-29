#include "cachesim.hpp"

#include <math.h>
#include <stdlib.h> 
#include <vector>
#include <iostream>

static const int addr_size = 64;

// struct block {
//     uint64_t tag, addr;
//     bool valid, dirty;
// };

struct set {
    std::vector<uint64_t> lru_stack;
    std::vector<std::vector<uint64_t>> blocks; /* 0 is addr, 1 is tag, 2 is valid, 3 is dirty */
    int length;

    set(int _length): length(_length) {
        lru_stack.resize(_length);
        blocks.resize(_length);
        for (int i = 0; i < _length; i++) {
            blocks[i].resize(4);
            blocks[i][0] = 0;
            blocks[i][1] = 0;
            blocks[i][2] = 0;
            blocks[i][3] = 0;
            lru_stack[i] = 0;
        }
    }
};

/* global cache variables */
std::vector<set> l1_cache;
std::vector<set> l2_cache;
std::vector<set> vi_cache;

int block_size, num_offset_bits;
int l1_num_ways, l1_cache_size, l1_num_sets, l1_num_index_bits, l1_num_tag_bits;
int l2_num_ways, l2_cache_size, l2_num_sets, l2_num_index_bits, l2_num_tag_bits;
int vi_num_ways, vi_cache_size, vi_num_tag_bits;
insert_policy_t l2_insert_policy;
bool l2_disabled, vi_disabled;

void set_mru(std::vector<uint64_t>& stack, uint64_t tag) {
	/* temporary array to hold values */
    std::vector<uint64_t> temp(stack.size() - 1);

	size_t j = 0;
	for (size_t i = 0; i < stack.size(); i++) {
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
	for (size_t k = 1; k < stack.size(); k++) {
		stack[k] = temp[k-1];
	}
}

void set_lru(std::vector<uint64_t>& stack, uint64_t tag) {
	/* temporary array to hold values */
    std::vector<uint64_t> temp(stack.size() - 1);

	size_t j = 0;
	for (size_t i = 0; i < stack.size(); i++) {
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
	for (size_t k = 0; k < stack.size() - 1; k++) {
		stack[k] = temp[k];
	}
}

void popoff_stack(std::vector<uint64_t>& stack, uint64_t tag) {
    /* remove hit block from victim lru stack */
    for (size_t i = 0; i < stack.size(); i++) {
        if (stack[i] == tag) {
            stack[i] = 0;
            break;
        }
    }

    /* temporary array to hold values */
    std::vector<uint64_t> temp(stack.size());

    /* move all 0 spots to bottom of stack */
    size_t j = 0;
    for (size_t i = 0; i < stack.size(); i++) {
        if (stack[i] != 0) {
            temp[j] = stack[i];
            j++;
        }
    }
}

uint64_t get_lru(std::vector<uint64_t>& stack) {
    /* loop until end of valid tags and return index */
    size_t i;
    for (i = 0; i < stack.size(); i++) {
        if (stack[i] == 0) break;
    }
    return stack[i - 1];
}

int available(std::vector<std::vector<uint64_t>>& set) {
    /* return index on first open block */
    for (size_t i = 0; i < set.size(); i++) {
        if (!set[i][2]) return i;
    }
    /* return -1 for no open blocks */
    return -1;
}

/* subroutine for initializing the cache simulator */
void sim_setup(sim_config_t *config) {
    /* initialize l1 global cache config values */
    block_size = pow(2, config->l1_config.b);
    num_offset_bits = config->l1_config.b;

    l1_num_ways = pow(2, config->l1_config.s);
    l1_cache_size = pow(2, config->l1_config.c);
    l1_num_sets = l1_cache_size / block_size / l1_num_ways;
    l1_num_index_bits = config->l1_config.c - config->l1_config.s - config->l1_config.b;
    l1_num_tag_bits = addr_size - (l1_num_index_bits + num_offset_bits);

    for (int i = 0; i < l1_num_sets; i++) {
        l1_cache.push_back(set(l1_num_ways));
    }

    /* initialize l2 global cache config values */
    l2_disabled = config->l2_config.disabled;
    if (!l2_disabled) {
        l2_num_ways = pow(2, config->l2_config.s);
        l2_cache_size = pow(2, config->l2_config.c);
        l2_num_sets = l2_cache_size / block_size / l2_num_ways;
        l2_num_index_bits = config->l2_config.c - config->l2_config.s - config->l2_config.b;
        l2_num_tag_bits = addr_size - (l2_num_index_bits + num_offset_bits);
        l2_insert_policy = config->l2_config.insert_policy;

        for (int i = 0; i < l2_num_sets; i++) {
            l2_cache.push_back(set(l2_num_ways));
        }
    }

    /* initialize victim global cache config values */
    vi_disabled = !(config->victim_cache_entries);
    if (!vi_disabled) {
        vi_num_ways = config->victim_cache_entries;
        vi_cache_size = block_size * vi_num_ways;
        vi_num_tag_bits = addr_size - num_offset_bits;
        vi_cache.push_back(set(vi_num_ways));
        // std::cout << vi_cache.size() << std::endl;
        // std::cout << vi_cache[0].blocks.size() << std::endl;
        // std::cout << vi_cache[0].blocks[0].size() << std::endl;
    }
}

/* subroutine that simulates the cache one trace event at a time */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    /* get tag, index */
    uint64_t l1_tag = (addr >> (l1_num_index_bits + num_offset_bits)) & ((1 << l1_num_tag_bits) - 1);
    uint64_t l1_index = (addr >> num_offset_bits) & ((1 << l1_num_index_bits) - 1);

    /* increment l1 accesses */
    stats->accesses_l1++;

    /* search l1 cache for tag */
    bool hit = false;
    int hit_block;
    for (hit_block = 0; hit_block < l1_num_ways; hit_block++) {
        if (l1_cache[l1_index].blocks[hit_block][1] == l1_tag && l1_cache[l1_index].blocks[hit_block][2]) {
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
            l1_cache[l1_index].blocks[hit_block][3] = true;

            /* increment writes */
            stats->writes++;
        }
        else {
            /* increment reads */
            stats->reads++;
        }

        /* set hit block to MRU */
        set_mru(l1_cache[l1_index].lru_stack, l1_tag);

        return;
    }

    /* function did not return, increment l1 misses */
    stats->misses_l1++;
    
    /* check if victim cache is enabled */
    uint64_t vi_tag;
    if (!vi_disabled) {
        /* update tag */
        vi_tag = (addr >> num_offset_bits) & ((1 << vi_num_tag_bits) - 1);

        // std::cout << std::endl << "CAN I KICK IT?! " << vi_tag << std::endl << "victim blocks - " << std::endl;
        // for (int i = 0; i < vi_num_ways; i++) {
        //     std::cout << vi_cache[0].blocks[i][1] << std::endl;
        // }
        // std::cout << "lru stack - " << std::endl;
        // for (int i = 0; i < vi_num_ways; i++) {
        //     std::cout << vi_cache[0].lru_stack[i] << std::endl;
        // }
        // std::cout << std::endl;

        /* search victim cache for tag */
        for (hit_block = 0; hit_block < vi_num_ways; hit_block++) {
            if (vi_cache[0].blocks[hit_block][1] == vi_tag && vi_cache[0].blocks[hit_block][2]) {
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
                vi_cache[0].blocks[hit_block][3] = true;

                /* increment writes */
                stats->writes++;
            }
            else {
                /* increment reads */
                stats->reads++;
            }

            /* see if set in l1 has empty spots */
            int open_block = available(l1_cache[l1_index].blocks);
            if (open_block < 0) {
                // std::cout << "NO FREE LUNCH " << vi_tag << std::endl;
                /* no open spots in l1 set, must evict - get the lru for the hit block's set in l1 */
                uint64_t l1_lru_tag = get_lru(l1_cache[l1_index].lru_stack);

                /* swap actual blocks in victim and l1 */
                for (int i = 0; i < l1_num_ways; i++) {
                    if (l1_cache[l1_index].blocks[i][1] == l1_lru_tag && l1_cache[l1_index].blocks[i][2]) {
                        /* exchange block info */
                        int temp_hit_dirty = vi_cache[0].blocks[hit_block][3];

                        vi_cache[0].blocks[hit_block][3] = l1_cache[l1_index].blocks[i][3];
                        vi_cache[0].blocks[hit_block][1] = (l1_cache[l1_index].blocks[i][0] >> num_offset_bits) & ((1 << vi_num_tag_bits) - 1);
                        vi_cache[0].blocks[hit_block][2] = true;
                        vi_cache[0].blocks[hit_block][0] = l1_cache[l1_index].blocks[i][0];

                        l1_cache[l1_index].blocks[i][3] = temp_hit_dirty;
                        l1_cache[l1_index].blocks[i][2] = true;
                        l1_cache[l1_index].blocks[i][1] = l1_tag;
                        l1_cache[l1_index].blocks[i][0] = addr;

                        break;
                    }
                }

                /* remove evicted block from l1 lru stack */
                popoff_stack(l1_cache[l1_index].lru_stack, l1_lru_tag);

                /* remove hit block from victim lru stack */
                popoff_stack(vi_cache[0].lru_stack, vi_tag);

                /* swap tags in victim and l1 lru stacks */
                // if (vi_cache[0].blocks[hit_block][1] == 35234205) std::cout << "FLAG 289" << std::endl;
                set_mru(vi_cache[0].lru_stack, vi_cache[0].blocks[hit_block][1]);
                set_mru(l1_cache[l1_index].lru_stack, l1_tag);

                return;
            }
            else {
                /* open spot available, save hit block in l1 */
                l1_cache[l1_index].blocks[open_block][1] = l1_tag;
                l1_cache[l1_index].blocks[open_block][0] = addr;
                l1_cache[l1_index].blocks[open_block][3] = vi_cache[0].blocks[hit_block][3];
                l1_cache[l1_index].blocks[open_block][2] = true;

                /* remove hit block from victim lru stack */
                // if (vi_tag == 35234205) std::cout << "FLAG 303" << std::endl;
                popoff_stack(vi_cache[0].lru_stack, vi_tag);

                /* remove hit block from victim block list */
                vi_cache[0].blocks[hit_block][2] = 0;

                /* set incoming hit block to mru in l1 */
                set_mru(l1_cache[l1_index].lru_stack, l1_tag);
            }
        }
    }

    /* victim cache disabled or function did not return, increment victim misses */
    stats->misses_victim_cache++;

    /* check if l2 cache is enabled */
    uint64_t l2_tag, l2_index;
    if (!l2_disabled) {
        /* increment r/w request stats */
        if (rw == WRITE) {
            stats->writes_l2++;
        }
        else {
            stats->reads_l2++;
        }

        /* update tag, index */
        l2_tag = (addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
        l2_index = (addr >> num_offset_bits) & ((1 << l2_num_index_bits) - 1);

        /* search l2 cache for tag */
        for (hit_block = 0; hit_block < l2_num_ways; hit_block++) {
            if (l2_cache[l2_index].blocks[hit_block][1] == l2_tag && l2_cache[l2_index].blocks[hit_block][2]) {
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
            popoff_stack(l2_cache[l2_index].lru_stack, l2_tag);
            
            /* remove hit block from l2 */
            l2_cache[l2_index].blocks[hit_block][2] = false;

            /* check if there are open spots in l1 set for l2 hit block */
            int open_block = available(l1_cache[l1_index].blocks);
            uint64_t l1_evicted_addr;
            bool l1_evicted_dirty;
            if (open_block < 0) {
                /* no spots available, get l1 lru tag */
                uint64_t l1_lru_tag = get_lru(l1_cache[l1_index].lru_stack);

                /* get index of l1 lru in blocks */
                int l1_lru;
                for (l1_lru = 0; l1_lru < l1_num_ways; l1_lru++) {
                    if (l1_cache[l1_index].blocks[l1_lru][1] == l1_lru_tag) break;
                }

                /* bring in l2 hit, evict l1 lru to victim (if enabled) */
                l1_evicted_addr = l1_cache[l1_index].blocks[l1_lru][0];
                l1_evicted_dirty = l1_cache[l1_index].blocks[l1_lru][3];

                l1_cache[l1_index].blocks[l1_lru][0] = addr;
                l1_cache[l1_index].blocks[l1_lru][1] = l1_tag;
                l1_cache[l1_index].blocks[l1_lru][2] = true;
                l1_cache[l1_index].blocks[l1_lru][3] = false;

                popoff_stack(l1_cache[l1_index].lru_stack, l1_lru_tag);
                set_mru(l1_cache[l1_index].lru_stack, l1_tag);
            }
            else {
                /* open spot in hit block's l1 set, bring in block */
                l1_cache[l1_index].blocks[open_block][0] = addr;
                l1_cache[l1_index].blocks[open_block][1] = l1_tag;
                l1_cache[l1_index].blocks[open_block][2] = true;
                l1_cache[l1_index].blocks[open_block][3] = false;

                set_mru(l1_cache[l1_index].lru_stack, l1_tag);

                return;
            }            

            /* now save l1 victim to victim cache - if enabled and there is a victim */
            uint64_t vi_evicted_addr;
            if (!vi_disabled) {
                /* check for open blocks in victim cache */
                int open_block = available(vi_cache[0].blocks);

                if (open_block < 0) {
                    /* no open blocks, need to evict victim lru */
                    uint64_t vi_lru_tag = get_lru(vi_cache[0].lru_stack);

                    /* get index of victim lru block */
                    int vi_lru;
                    for (vi_lru = 0; vi_lru < vi_num_ways; vi_lru++) {
                        if (vi_cache[0].blocks[vi_lru][1] == vi_lru_tag) break;
                    }

                    /* save evicted victim lru to temp block */
                    vi_evicted_addr = vi_cache[0].blocks[vi_lru][0];

                    /* update victim cache block with evicted l1 block info */
                    vi_cache[0].blocks[vi_lru][0] = l1_evicted_addr;
                    vi_cache[0].blocks[vi_lru][1] = (l1_evicted_addr >> num_offset_bits) & ((1 << vi_num_tag_bits) - 1);
                    vi_cache[0].blocks[vi_lru][3] = l1_evicted_dirty;
                    vi_cache[0].blocks[vi_lru][2] = true;

                    // if (vi_cache[0].blocks[vi_lru][1] == 35234205) std::cout << "FLAG 424" << std::endl;

                    /* remove victim cache lru from lru stack */
                    popoff_stack(vi_cache[0].lru_stack, vi_lru_tag);

                    /* set l1 victim tag, now in victim cache, to mru */
                    set_mru(vi_cache[0].lru_stack, vi_cache[0].blocks[vi_lru][1]);
                }
                else {
                    /* open spot in victim cache, bring in l1 victim */
                    vi_cache[0].blocks[open_block][0] = l1_evicted_addr;
                    vi_cache[0].blocks[open_block][1] = (l1_evicted_addr >> num_offset_bits) & ((1 << vi_num_tag_bits) - 1);;
                    vi_cache[0].blocks[open_block][2] = true;
                    vi_cache[0].blocks[open_block][3] = l1_evicted_dirty;

                    /* set newly filled block to mru */
                    // if (vi_cache[0].blocks[open_block][1] == 35234205) std::cout << "FLAG 440" << std::endl;
                    set_mru(vi_cache[0].lru_stack, vi_cache[0].blocks[open_block][1]);

                    return;
                }
            }

            /* save either l1 lru or victim lru to l2 */
            uint64_t victim_index;
            if (vi_evicted_addr == 0) {
                /* current incoming block is from l1 */
                victim_index = (l1_evicted_addr >> num_offset_bits) & ((1 << l2_num_index_bits) - 1);
            }
            else {
                /* current incoming block is from the victim cache */
                victim_index = (vi_evicted_addr >> num_offset_bits) & ((1 << l2_num_index_bits) - 1);
            }
            
            /* check for open blocks in l2 cache */
            open_block = available(l2_cache[victim_index].blocks);

            if (open_block < 0) {
                /* no open blocks, need to evict l2 block */
                uint64_t l2_lru_tag = get_lru(l2_cache[victim_index].lru_stack);

                /* get index of victim lru block */
                int l2_lru;
                for (l2_lru = 0; l2_lru < l2_num_ways; l2_lru++) {
                    if (l2_cache[0].blocks[l2_lru][1] == l2_lru_tag) break;
                }

                /* update l2 cache block with evicted l1/victim block info */
                if (vi_evicted_addr == 0) {
                    l2_cache[victim_index].blocks[l2_lru][0] = l1_evicted_addr;
                    l2_cache[victim_index].blocks[l2_lru][1] = (l1_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
                    l2_cache[victim_index].blocks[l2_lru][3] = false;
                    l2_cache[victim_index].blocks[l2_lru][2] = true;
                }
                else {
                    l2_cache[victim_index].blocks[l2_lru][0] = vi_evicted_addr;
                    l2_cache[victim_index].blocks[l2_lru][1] = (vi_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
                    l2_cache[victim_index].blocks[l2_lru][3] = false;
                    l2_cache[victim_index].blocks[l2_lru][2] = true;
                }

                /* increment write backs as victim block is no longer dirty in l2 */
                stats->write_backs_l1_or_victim_cache++;

                /* remove l2 cache lru from lru stack */
                popoff_stack(l2_cache[victim_index].lru_stack, l2_lru_tag);

                /* set l1/victim lru tag, now in l2 cache, to mru OR lru depending on insertion policy */
                if (l2_insert_policy == INSERT_POLICY_MIP) {
                    set_mru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[l2_lru][1]);
                }
                else {
                    set_lru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[l2_lru][1]);
                }
            }
            else {
                /* open spot in l2 cache, bring in l1/victim lru */
                if (vi_evicted_addr == 0) {
                    l2_cache[victim_index].blocks[open_block][0] = l1_evicted_addr;
                    l2_cache[victim_index].blocks[open_block][1] = (l1_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
                    l2_cache[victim_index].blocks[open_block][3] = false;
                    l2_cache[victim_index].blocks[open_block][2] = true;
                }
                else {
                    l2_cache[victim_index].blocks[open_block][0] = vi_evicted_addr;
                    l2_cache[victim_index].blocks[open_block][1] = (vi_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
                    l2_cache[victim_index].blocks[open_block][3] = false;
                    l2_cache[victim_index].blocks[open_block][2] = true;
                }

                /* increment write backs as victim block is no longer dirty in l2 */
                stats->write_backs_l1_or_victim_cache++;

                /* set newly filled block to mru OR lru depending on insertion policy */
                if (l2_insert_policy == INSERT_POLICY_MIP) {
                    set_mru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[open_block][1]);
                }
                else {
                    set_lru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[open_block][1]);
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
    int open_block = available(l1_cache[l1_index].blocks);
    uint64_t l1_evicted_addr;
    bool l1_evicted_dirty;
    if (open_block < 0) {
        /* no spots available, get l1 lru tag */
        uint64_t l1_lru_tag = get_lru(l1_cache[l1_index].lru_stack);

        /* get index of l1 lru in blocks */
        int l1_lru;
        for (l1_lru = 0; l1_lru < l1_num_ways; l1_lru++) {
            if (l1_cache[l1_index].blocks[l1_lru][1] == l1_lru_tag) break;
        }

        /* bring in DRAM hit, evict l1 lru to victim (if enabled) */
        l1_evicted_addr = l1_cache[l1_index].blocks[l1_lru][0];
        l1_evicted_dirty = l1_cache[l1_index].blocks[l1_lru][3];

        l1_cache[l1_index].blocks[l1_lru][0] = addr;
        l1_cache[l1_index].blocks[l1_lru][1] = l1_tag;
        l1_cache[l1_index].blocks[l1_lru][2] = true;
        l1_cache[l1_index].blocks[l1_lru][3] = false;

        popoff_stack(l1_cache[l1_index].lru_stack, l1_lru_tag);
        set_mru(l1_cache[l1_index].lru_stack, l1_tag);
    }
    else {
        /* save block from memory into open block */
        l1_cache[l1_index].blocks[open_block][1] = l1_tag;
        l1_cache[l1_index].blocks[open_block][0] = addr;
        l1_cache[l1_index].blocks[open_block][2] = true;
        l1_cache[l1_index].blocks[open_block][3] = false;

        /* set incoming block as the mru */
        set_mru(l1_cache[l1_index].lru_stack, l1_tag);

        return;
    }

    /* now check victim cache for open spots - if there was an eviction in previous stage */
    uint64_t vi_evicted_addr;
    if (!vi_disabled) {
        /* check for open blocks in victim cache */
        int open_block = available(vi_cache[0].blocks);

        if (open_block < 0) {
            /* no open blocks, need to evict victim lru */
            uint64_t vi_lru_tag = get_lru(vi_cache[0].lru_stack);
            // std::cout << "vi_lru_tag: " << vi_lru_tag << std::endl;

            // std::cout << "vi lru stack: " << std::endl;
            // for (int i = 0; i < vi_num_ways; i++) {
            //     std::cout << vi_cache[0].lru_stack[i] << std::endl;
            // }

            /* get index of victim lru block */
            int vi_lru;
            // std::cout << "vi blocks: " << std::endl;
            // for (int i = 0; i < vi_num_ways; i++) {
            //     if (vi_cache[0].blocks[i][2] == true) std::cout << vi_cache[0].blocks[i][1] << std::endl;
            // }
            for (vi_lru = 0; vi_lru < vi_num_ways; vi_lru++) {
                // std::cout << vi_cache[0].blocks[vi_lru][1] << std::endl;
                if (vi_cache[0].blocks[vi_lru][1] == vi_lru_tag) break;
            }

            /* save evicted victim lru to temp block */
            vi_evicted_addr = vi_cache[0].blocks[vi_lru][0];

            /* update victim cache block with evicted l1 block info */
            vi_cache[0].blocks[vi_lru][0] = l1_evicted_addr;
            vi_cache[0].blocks[vi_lru][1] = (l1_evicted_addr >> num_offset_bits) & ((1 << vi_num_tag_bits) - 1);
            vi_cache[0].blocks[vi_lru][3] = l1_evicted_dirty;
            vi_cache[0].blocks[vi_lru][2] = true;

            /* remove victim cache lru from lru stack */
            // if (vi_lru_tag == 35234205) std::cout << "FLAG 617" << std::endl;
            popoff_stack(vi_cache[0].lru_stack, vi_lru_tag);

            /* set l1 victim tag, now in victim cache, to mru */
            set_mru(vi_cache[0].lru_stack, vi_cache[0].blocks[vi_lru][1]);
        }
        else {
            /* open spot in victim cache, bring in l1 victim */
            vi_cache[0].blocks[open_block][0] = l1_evicted_addr;
            vi_cache[0].blocks[open_block][1] = (l1_evicted_addr >> num_offset_bits) & ((1 << vi_num_tag_bits) - 1);;
            vi_cache[0].blocks[open_block][2] = true;
            vi_cache[0].blocks[open_block][3] = l1_evicted_dirty;

            /* set newly filled block to mru */
            // if (vi_cache[0].blocks[open_block][1] == 35234205) std::cout << "FLAG 631" << std::endl;
            set_mru(vi_cache[0].lru_stack, vi_cache[0].blocks[open_block][1]);

            return;
        }
    }

    /* finally - save evicted block to l2 if needed and enabled; otherwise block just goes back to DRAM */
    uint64_t victim_index;
    if (vi_evicted_addr == 0) {
        /* current incoming block is from l1 */
        victim_index = (l1_evicted_addr >> num_offset_bits) & ((1 << l2_num_index_bits) - 1);
    }
    else {
        /* current incoming block is from the victim cache */
        victim_index = (vi_evicted_addr >> num_offset_bits) & ((1 << l2_num_index_bits) - 1);
    }
    
    /* check for open blocks in l2 cache */
    open_block = available(l2_cache[victim_index].blocks);

    if (open_block < 0) {
        /* no open blocks, need to evict l2 block */
        uint64_t l2_lru_tag = get_lru(l2_cache[victim_index].lru_stack);

        /* get index of victim lru block */
        int l2_lru;
        for (l2_lru = 0; l2_lru < l2_num_ways; l2_lru++) {
            if (l2_cache[victim_index].blocks[l2_lru][1] == l2_lru_tag) break;
        }

        if (l2_lru == l2_num_ways) {
            std::cout << "FUCK " << addr << std::endl;
            for (int i = 0; i < l2_num_ways; i++) {
                bool match = false;
                for (int g = 0; g < l2_num_ways; g++) {
                    if (l2_cache[victim_index].blocks[i][1] == l2_cache[victim_index].lru_stack[g] && l2_cache[victim_index].blocks[i][2]) {
                        match = true;
                    }
                }
                if (!match) {
                    std::cout << "FUCK PT 2 " << addr << std::endl;
                }
            }
        }

        /* update l2 cache block with evicted l1/victim block info */
        if (vi_evicted_addr == 0) {
            l2_cache[victim_index].blocks[l2_lru][0] = l1_evicted_addr;
            l2_cache[victim_index].blocks[l2_lru][1] = (l1_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
            l2_cache[victim_index].blocks[l2_lru][3] = false;
            l2_cache[victim_index].blocks[l2_lru][2] = true;
        }
        else {
            l2_cache[victim_index].blocks[l2_lru][0] = vi_evicted_addr;
            l2_cache[victim_index].blocks[l2_lru][1] = (vi_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
            l2_cache[victim_index].blocks[l2_lru][3] = false;
            l2_cache[victim_index].blocks[l2_lru][2] = true;
        }

        /* increment write backs as victim block is no longer dirty in l2 */
        stats->write_backs_l1_or_victim_cache++;

        /* remove l2 cache lru from lru stack */
        popoff_stack(l2_cache[victim_index].lru_stack, l2_lru_tag);

        /* set l1/victim lru tag, now in l2 cache, to mru OR lru depending on insertion policy */
        if (l2_insert_policy == INSERT_POLICY_MIP) {
            set_mru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[l2_lru][1]);
        }
        else {
            set_lru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[l2_lru][1]);
        }
    }
    else {
        /* open spot in l2 cache, bring in l1/victim lru */
        if (vi_evicted_addr == 0) {
            l2_cache[victim_index].blocks[open_block][0] = l1_evicted_addr;
            l2_cache[victim_index].blocks[open_block][1] = (l1_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
            l2_cache[victim_index].blocks[open_block][3] = false;
            l2_cache[victim_index].blocks[open_block][2] = true;
        }
        else {
            l2_cache[victim_index].blocks[open_block][0] = vi_evicted_addr;
            l2_cache[victim_index].blocks[open_block][1] = (vi_evicted_addr >> (l2_num_index_bits + num_offset_bits)) & ((1 << l2_num_tag_bits) - 1);
            l2_cache[victim_index].blocks[open_block][3] = false;
            l2_cache[victim_index].blocks[open_block][2] = true;
        }

        /* increment write backs as victim block is no longer dirty in l2 */
        stats->write_backs_l1_or_victim_cache++;

        /* set newly filled block to mru OR lru depending on insertion policy */
        if (l2_insert_policy == INSERT_POLICY_MIP) {
            set_mru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[open_block][1]);
        }
        else {
            set_lru(l2_cache[victim_index].lru_stack, l2_cache[victim_index].blocks[open_block][1]);
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

    // /* free blocks */
    // for (int i = 0; i < l1_num_sets; i++) {
    //     for (int j = 0; j < l1_num_ways; j++) {
    //         delete l1_cache[i].blocks[j];
    //     }
    // }

    // for (int i = 0; i < l2_num_sets; i++) {
    //     for (int j = 0; j < l2_num_ways; j++) {
    //         delete l2_cache[i].blocks[j];
    //     }
    // }

    // for (int j = 0; j < vi_num_ways; j++) {
    //     delete vi_cache[0].blocks[j];
    // }
}

