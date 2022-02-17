#include "cachesim.hpp"

#include <math.h>
#include <stdlib.h> 
#include <vector>
#include <iostream>

using namespace std;

/* block class to hold tag, dirty bit, full address, and valid bit */
class Block {
    public:
        Block(): tag(0), addr(0), valid(false), dirty(false) {}

        uint64_t tag;
        uint64_t addr;
        bool valid;
        bool dirty;

        /* wipe out all fields in the block */
        void clear_block() {
            this->tag = 0;
            this->addr = 0;
            this->valid = false;
            this->dirty = false;
        }
};

/* set class holding the lru stack and block list */
class Set {
    public:
        Set(int _length): length(_length) {
            /* initialize lru_stack block objects in block vector */
            lru_stack.resize(_length);
            blocks.resize(_length);
            for (int i = 0; i < _length; i++) {
                blocks.push_back(Block());
                lru_stack.push_back(0);
            }
        }

        void set_mru(uint64_t tag);
        void set_lru(uint64_t tag);
        uint64_t get_lru();
        int check_availability();
        int search_for_hit(uint64_t tag);
        void delete_block(uint64_t tag);

        vector<uint64_t> lru_stack;
        vector<Block> blocks;
        int length;
};

/* set the mru with tag parameter for the set object */
void Set::set_mru(uint64_t tag) {
    /* determine if the tag is even in the shack */
    bool in_stack = false;
    for (int i = 0; i < this->length; i++) { 
        if (tag == this->lru_stack[i]) in_stack = true;
    }

    if (in_stack) {
        /* tag is in stack, copy other tags and save them again behind mru tag */
        uint64_t temp = new uint64_t[this->length];
        int j = 0;
        for (int i = 0; i < this->length; i++) {
            if (i == j && i == (this->length - 1)) break;
            
            if (this->lru_stack[i] != tag && this->lru_stack[i] != 0) {
                temp[j] = this->lru_stack[i];
                j++;
            }
        }

        this->lru_stack[0] = tag;

        for (int k = 1; k < this->length; k++) {
            this->lru_stack[k] = temp[k-1];
        }

        delete temp;
    }
    else {
        /*  tag not in stack, set as lru and call again */
        this->lru_stack[this->length - 1] = tag;
        this->set_mru(tag);
    }
}

void Set::set_lru(uint64_t tag) {
    /* determine if the tag is even in the shack */
    bool in_stack = false;
    for (int i = 0; i < this->length; i++) { 
        if (tag == this->lru_stack[i]) in_stack = true;
    }

    if (in_stack) {
        /* tag is in stack, copy other tags and save them again ahead lru tag */
        uint64_t temp = new uint64_t[this->length];
        int j = 0;
        for (int i = 0; i < this->length; i++) {
            if (i == j && i == (this->length - 1)) break;

            if (this->lru_stack[i] != tag && this->lru_stack[i] != 0) {
                temp[j] = this->lru_stack[i];
                j++;
            }
        }
        
        this->lru_stack[this->length - 1] = tag;

        for (int k = 0; k < this->length - 1; k++) {
            this->lru_stack[k] = temp[k];
        }

        delete temp;
    }
    else {
        /*  tag not in stack, set as lru and call again */
        this->lru_stack[this->length - 1] = tag;
        this->set_lru(tag);
    }
}

/* returns the lru tag of the set object */
uint64_t Set::get_lru() {
    int i;
    for (i = 0; i < this->length; i++) {
        if (this->lru_stack[i] == 0) break;
    }
    return this->lru_stack[i - 1];
}

/* returns the index in the block vector of an open spot (if available */
int Set::check_availability() {
    for (int i = 0; i < this->length; i++) {
        if (!this->blocks[i].valid) return i;
    }
    return -1;
}
 
/* traverses set's block vector to see if the tag is present */
int Set::search_for_hit(uint64_t tag) {
    for (int i = 0; i < this->length; i++) {
        if (this->blocks[i].tag == tag && this->blocks[i].valid) {
            return i;
        }
    }
    return -1;
}

/* clears out the desired block and removes its tag from the lru_stack */
void Set::delete_block(uint64_t tag) {
    for (int i = 0; i < this->length; i++) {
        if (this->lru_stack[i] == tag) {
            this->lru_stack[i] = 0;
        }
        if (this->blocks[i].tag == tag) {
            this->blocks[i].clear_block();
        }
    }
}

/* cache object holding cache config values and vector of sets */
class Cache {
    public:
        Cache() {}

        int ways;
        int sets;
        int cache_size;
        int index_size;
        int tag_size;
        bool disabled;

        vector<Set> set_heap;

        /* initializes the set vectors of the cache */
        void initialize_cache() {
            for (int i = 0; i < this->sets; i++) {
                this->set_heap.push_back(Set(this->ways));
            }
        }
};

/* global cache variables */
Cache L1 = Cache();
Cache L2 = Cache();
Cache VC = Cache();

/* useful non-object-specific variables */
static const int addr_size = 64;
int block_size, num_offset_bits;
insert_policy_t insert_policy;

/* subroutine to set up cache objects before simulation begins */
void sim_setup(sim_config_t *config) {
    /* initialize global cache config values */
    block_size = pow(2, config->l1_config.b);
    num_offset_bits = config->l1_config.b;

    /* initialize L1 cache config vals */
    L1.cache_size = pow(2, config->l1_config.c);
    L1.ways = pow(2, config->l1_config.s);
    L1.sets = L1.cache_size / block_size / L1.ways;
    L1.index_size = config->l1_config.c - config->l1_config.s - config->l1_config.b;
    L1.tag_size = addr_size - (L1.index_size + num_offset_bits);
    L1.initialize_cache();

    /* initialize l2 global cache config values */
    L2.disabled = config->l2_config.disabled;
    if (!L2.disabled) {
        L2.ways = pow(2, config->l2_config.s);
        L2.cache_size = pow(2, config->l2_config.c);
        L2.sets = L2.cache_size / block_size / L2.ways;
        L2.index_size = config->l2_config.c - config->l2_config.s - config->l2_config.b;
        L2.tag_size = addr_size - (L2.index_size + num_offset_bits);
        insert_policy = config->l2_config.insert_policy;
        L2.initialize_cache();
    }

    /* initialize victim global cache config values */
    VC.disabled = !(config->victim_cache_entries);
    if (!VC.disabled) {
        VC.ways = config->victim_cache_entries;
        VC.cache_size = block_size * VC.ways;
        VC.tag_size = addr_size - num_offset_bits;
        VC.index_size = 0;
        VC.sets = 1;
        VC.initialize_cache();
    }
}

/* subroutine to simlulate a cache read/write, while keeping track of statistics */
void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    // cout << addr << endl;
    stats->accesses_l1++;

    /* compute tag and index from address for L1 */
    uint64_t tag_L1 = (addr >> (L1.index_size + num_offset_bits)) & ((1 << L1.tag_size) - 1);
    uint64_t index_L1 = (addr >> num_offset_bits) & ((1 << L1.index_size) - 1);

    /* search for a hit in L1 cache */
    int hit_L1 = L1.set_heap[index_L1].search_for_hit(tag_L1);

    if (hit_L1 > 0) {
        /* tag was found, flip dirty bit if writing, set as mru */
        stats->hits_l1++;

        if (rw == WRITE) {
            L1.set_heap[index_L1].blocks[hit_L1].dirty = true;
            stats->writes++;
        }
        else stats->reads++;

        L1.set_heap[index_L1].set_mru(tag_L1);
        return;
    }

    /* L1 cache missed */
    stats->misses_l1++;
    
    uint64_t tag_VC;
    /* check that VC is enabled */
    if (!VC.disabled) {
        /* get new tag value of address for victim cache */
        tag_VC = (addr >> num_offset_bits) & ((1 << VC.tag_size) - 1);
        
        /* search VC for a hit */
        int hit_VC = VC.set_heap[0].search_for_hit(tag_VC);

        if (hit_VC > 0) {
            /* tag was found, now have to move hit block into L1 */
            stats->hits_victim_cache++;

            if (rw == WRITE) {
                /* set dirty bit for write-back */
                VC.set_heap[0].blocks[hit_VC].dirty = true;
                stats->writes++;
            }
            else stats->reads++;

            /* check to see if L1 has open blocks already in the right set */
            int open_block = L1.set_heap[index_L1].check_availability();
            if (open_block < 0) {
                /* no open blocks found, need to evict L1 LRU of the set */
                uint64_t tag_LRU_L1 = L1.set_heap[index_L1].get_lru();
 
                for (int i = 0; i < L1.ways; i++) {
                    /* search for the LRU block by tag */
                    if (L1.set_heap[index_L1].blocks[i].tag == tag_LRU_L1 && L1.set_heap[index_L1].blocks[i].valid) {
                        /* swap parameters of the incoming hit block and LRU L1 block */
                        bool temp_dirty = VC.set_heap[0].blocks[hit_VC].dirty;

                        VC.set_heap[0].blocks[hit_VC].dirty = L1.set_heap[index_L1].blocks[i].dirty;
                        VC.set_heap[0].blocks[hit_VC].tag = (L1.set_heap[index_L1].blocks[i].addr >> num_offset_bits) & ((1 << VC.tag_size) - 1);
                        VC.set_heap[0].blocks[hit_VC].valid = true;
                        VC.set_heap[0].blocks[hit_VC].addr = L1.set_heap[index_L1].blocks[i].addr;

                        L1.set_heap[index_L1].blocks[i].dirty = temp_dirty;
                        L1.set_heap[index_L1].blocks[i].valid = true;
                        L1.set_heap[index_L1].blocks[i].tag = tag_L1;
                        L1.set_heap[index_L1].blocks[i].addr = addr;

                        /* set the previous L1 LRU as the new mru of the victim cache */
                        VC.set_heap[0].set_mru(VC.set_heap[0].blocks[hit_VC].tag);

                        /* set the hit block as the new MRU of the L1 cache */
                        L1.set_heap[index_L1].set_mru(tag_L1);

                        break;
                    }
                }

                return;
            }
            else {
                /* open spot found - just save block data and, clear out old block in victim, and set as mru */
                L1.set_heap[index_L1].blocks[open_block].tag = tag_L1;
                L1.set_heap[index_L1].blocks[open_block].addr = addr;
                L1.set_heap[index_L1].blocks[open_block].dirty = VC.set_heap[0].blocks[hit_VC].dirty;
                L1.set_heap[index_L1].blocks[open_block].valid = true;

                VC.set_heap[0].delete_block(tag_VC);

                L1.set_heap[index_L1].set_mru(tag_L1);
            }
        }
    }

    /* victim cache missed */
    stats->misses_victim_cache++;

    uint64_t tag_L2, index_L2;
    /* check that L2 is enabled */
    if (!L2.disabled) {
        if (rw == WRITE) stats->writes_l2++;
        else stats->reads_l2++;
        
        /* compute new tag and index values for the address in L2 */
        tag_L2 = (addr >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
        index_L2 = (addr >> num_offset_bits) & ((1 << L2.index_size) - 1);

        /* search for a hit in L2 */
        int hit_L2 = L2.set_heap[index_L2].search_for_hit(tag_L2);

        if (hit_L2 > 0) {
            /* hi was found, now move L2 block into L1 */
            if (rw == WRITE) stats->writes++;
            else {
                stats->reads++;
                stats->read_hits_l2++;
            }

            /* remove hit block in L2 before saving into L1 */
            L2.set_heap[index_L2].delete_block(tag_L2);

            uint64_t addr_evicted_L1;
            bool dirty_evicted_L1;

            /* search to see if L1 has any open blocks in the set of the incoming block */
            int open_block = L2.set_heap[index_L2].check_availability();
            if (open_block < 0) {
                /* no open blocks, need to evict the L1 LRU */
                uint64_t tag_LRU_L1 = L1.set_heap[index_L1].get_lru();

                /* find the index in vector of blocks of the LRU */
                int block_LRU_L1;
                for (block_LRU_L1 = 0; block_LRU_L1 < L1.ways; block_LRU_L1++) {
                    if (L1.set_heap[index_L1].blocks[block_LRU_L1].tag == tag_LRU_L1) break;
                }

                /* save the evicted block's data before kicking it out */
                addr_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].addr;
                dirty_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].dirty;

                /* clear out the block from the block object and the lru_stack */
                L1.set_heap[index_L1].delete_block(tag_LRU_L1);

                /* save incoming hit block data into previous L1 LRU spot */
                L1.set_heap[index_L1].blocks[block_LRU_L1].addr = addr;
                L1.set_heap[index_L1].blocks[block_LRU_L1].tag = tag_L1;
                L1.set_heap[index_L1].blocks[block_LRU_L1].valid = true;
                L1.set_heap[index_L1].blocks[block_LRU_L1].dirty = false;
                
                /* set hit block as MRU in L1 */
                L1.set_heap[index_L1].set_mru(tag_L1);
            }
            else {
                /* open block found - just save L2 hit block data into empty block and set as MRU */
                L1.set_heap[index_L1].blocks[open_block].addr = addr;
                L1.set_heap[index_L1].blocks[open_block].tag = tag_L1;
                L1.set_heap[index_L1].blocks[open_block].valid = true;
                L1.set_heap[index_L1].blocks[open_block].dirty = false;

                L1.set_heap[index_L1].set_mru(tag_L1);

                return;
            }            

            uint64_t addr_evicted_VC;
            /* handle case of L1 LRU is evicted to victim cache */
            if (!VC.disabled) {
                /* check to see if victim cache has open blocks */
                int open_block = VC.set_heap[0].check_availability();

                if (open_block < 0) {
                    /* no open blocks, need to evict and write back to L2 */
                    uint64_t tag_LRU_VC = VC.set_heap[0].get_lru();

                    /* find index of victim LRU in block vector */
                    int block_LRU_VC;
                    for (block_LRU_VC = 0; block_LRU_VC < VC.ways; block_LRU_VC++) {
                        if (VC.set_heap[0].blocks[block_LRU_VC].tag == tag_LRU_VC) break;
                    }

                    /* save address of victim before clearing out */
                    addr_evicted_VC = VC.set_heap[0].blocks[block_LRU_VC].addr;

                    /* clear out evicted block from block vector and stack */
                    VC.set_heap[0].delete_block(tag_LRU_VC);

                    /* set the L1 LRU evicted block data to the previous LRU of the victim cache */
                    VC.set_heap[0].blocks[block_LRU_VC].addr = addr_evicted_L1;
                    VC.set_heap[0].blocks[block_LRU_VC].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);
                    VC.set_heap[0].blocks[block_LRU_VC].dirty = dirty_evicted_L1;
                    VC.set_heap[0].blocks[block_LRU_VC].valid = true;

                    /* set the L1 victim to the MRU of the victim cache */
                    VC.set_heap[0].set_mru( VC.set_heap[0].blocks[block_LRU_VC].tag);
                }
                else {
                    /* open block was found - just save parameters of evicted L1 block and set as MRU */
                    VC.set_heap[0].blocks[open_block].addr = addr_evicted_L1;
                    VC.set_heap[0].blocks[open_block].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);;
                    VC.set_heap[0].blocks[open_block].valid = true;
                    VC.set_heap[0].blocks[open_block].dirty = dirty_evicted_L1;

                    VC.set_heap[0].set_mru(VC.set_heap[0].blocks[open_block].tag);

                    return;
                }
            }

            /* now deal with a possible victim from the victim cache being written back to L2 , or the L1 victim if Vc is disabled*/
            uint64_t victim_index;
            if (addr_evicted_VC == 0) {
                victim_index = (addr_evicted_L1 >> num_offset_bits) & ((1 << L2.index_size) - 1);
            }
            else {
                victim_index = (addr_evicted_VC >> num_offset_bits) & ((1 << L2.index_size) - 1);
            }
            
            /* look for an open block in L2 */
            open_block = L2.set_heap[victim_index].check_availability();

            if (open_block < 0) {
                /* no open block found, evict the LRU of L2 */
                uint64_t tag_LRU_L2 = L2.set_heap[victim_index].get_lru();

                /* find index of LRU in block vector */
                int block_LRU_L2;
                for (block_LRU_L2 = 0; block_LRU_L2 < L2.ways; block_LRU_L2++) {
                    if (L2.set_heap[victim_index].blocks[block_LRU_L2].tag == tag_LRU_L2) break;
                }

                /* clear out the L2 LRU block */
                L2.set_heap[victim_index].delete_block(tag_LRU_L2);

                /* save L1 or victim cache's evicted block data to the just-evicted LRU of L2 */
                if (addr_evicted_VC == 0) {
                    L2.set_heap[victim_index].blocks[block_LRU_L2].addr = addr_evicted_L1;
                    L2.set_heap[victim_index].blocks[block_LRU_L2].tag = (addr_evicted_L1 >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
                    L2.set_heap[victim_index].blocks[block_LRU_L2].dirty = false;
                    L2.set_heap[victim_index].blocks[block_LRU_L2].valid = true;
                }
                else {
                    L2.set_heap[victim_index].blocks[block_LRU_L2].addr = addr_evicted_VC;
                    L2.set_heap[victim_index].blocks[block_LRU_L2].tag = (addr_evicted_VC >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
                    L2.set_heap[victim_index].blocks[block_LRU_L2].dirty = false;
                    L2.set_heap[victim_index].blocks[block_LRU_L2].valid = true;
                }

                stats->write_backs_l1_or_victim_cache++;

                /* deteremine whether to set the inco,ing victim for higher caches to the MRU or LRU */
                if (insert_policy == INSERT_POLICY_MIP) {
                    L2.set_heap[victim_index].set_mru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
                }
                else {
                    L2.set_heap[victim_index].set_lru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
                }
            }
            else {
                /* open block found, save victim to empty block and set as LRU or MRU depending on policy */
                if (addr_evicted_VC == 0) {
                    L2.set_heap[victim_index].blocks[open_block].addr = addr_evicted_L1;
                    L2.set_heap[victim_index].blocks[open_block].tag = (addr_evicted_L1 >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
                    L2.set_heap[victim_index].blocks[open_block].dirty = false;
                    L2.set_heap[victim_index].blocks[open_block].valid = true;
                }
                else {
                    L2.set_heap[victim_index].blocks[open_block].addr = addr_evicted_VC;
                    L2.set_heap[victim_index].blocks[open_block].tag = (addr_evicted_VC >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
                    L2.set_heap[victim_index].blocks[open_block].dirty = false;
                    L2.set_heap[victim_index].blocks[open_block].valid = true;
                }

                stats->write_backs_l1_or_victim_cache++;

                if (insert_policy == INSERT_POLICY_MIP) {
                    L2.set_heap[victim_index].set_mru(L2.set_heap[victim_index].blocks[open_block].tag);
                }
                else {
                    L2.set_heap[victim_index].set_lru(L2.set_heap[victim_index].blocks[open_block].tag);
                }

                return;
            }

            return;
        }
    }

    if (rw == READ) stats->read_misses_l2++;

    /* total cache miss - bring block into L1 from DRAM */

    uint64_t addr_evicted_L1;
    bool dirty_evicted_L1;

    /* check to see if L1 has open blocks */
    int open_block = L1.set_heap[index_L1].check_availability();
    if (open_block < 0) {
        /* no openings, need to evict LRU */
        uint64_t tag_LRU_L1 = L1.set_heap[index_L1].get_lru();
        
        /* find index of LRU block in block vector */
        int block_LRU_L1;
        for (block_LRU_L1 = 0; block_LRU_L1 < L1.ways; block_LRU_L1++) {
            if (L1.set_heap[index_L1].blocks[block_LRU_L1].tag == tag_LRU_L1) break;
        }
        
        /* save data of the soon-to-be-evicted block */
        addr_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].addr;
        dirty_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].dirty;
        
        /* clear out block */
        L1.set_heap[index_L1].delete_block(tag_LRU_L1);

        /* set incoming block pulled from DRAM into position of previous L1 lru */
        L1.set_heap[index_L1].blocks[block_LRU_L1].addr = addr;
        L1.set_heap[index_L1].blocks[block_LRU_L1].tag = tag_L1;
        L1.set_heap[index_L1].blocks[block_LRU_L1].valid = true;
        L1.set_heap[index_L1].blocks[block_LRU_L1].dirty = false;

        /* set newly brought block to MRU */
        L1.set_heap[index_L1].set_mru(tag_L1);
    }
    else {
        /* open block available - save block parameters and set as MRU */
        L1.set_heap[index_L1].blocks[open_block].tag = tag_L1;
        L1.set_heap[index_L1].blocks[open_block].addr = addr;
        L1.set_heap[index_L1].blocks[open_block].valid = true;
        L1.set_heap[index_L1].blocks[open_block].dirty = false;

        L1.set_heap[index_L1].set_mru(tag_L1);

        return;
    }

    uint64_t addr_evicted_VC;
    /* check if victim cache is enabled */
    if (!VC.disabled) {
        /* have to deal with possible victim from L1, check to see if victim cache has open spots */
        int open_block = VC.set_heap[0].check_availability();
        if (open_block < 0) {
            /* no open spots, need to evict LRU */
            uint64_t tag_LRU_VC = VC.set_heap[0].get_lru();
            
            /* find index of LRU in block vector */
            int block_LRU_VC;
            for (block_LRU_VC = 0; block_LRU_VC < VC.ways; block_LRU_VC++) {
                if (VC.set_heap[0].blocks[block_LRU_VC].tag == tag_LRU_VC) break;
            }

            /* save address of soon-to-be-evicted block */
            addr_evicted_VC = VC.set_heap[0].blocks[block_LRU_VC].addr;

            /* clear out block from lru_stack and block vector */
            VC.set_heap[0].delete_block(tag_LRU_VC);

            /* save L1 evicted block to just evicted VC block */
            VC.set_heap[0].blocks[block_LRU_VC].addr = addr_evicted_L1;
            VC.set_heap[0].blocks[block_LRU_VC].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);
            VC.set_heap[0].blocks[block_LRU_VC].dirty = dirty_evicted_L1;
            VC.set_heap[0].blocks[block_LRU_VC].valid = true;
            
            /* set previous L1 LRU to MRU in VC */
            VC.set_heap[0].set_mru(VC.set_heap[0].blocks[block_LRU_VC].tag);
        }
        else {
            /* open spot found in VC - save L1 victim data and set as MRU in VC */
            VC.set_heap[0].blocks[open_block].addr = addr_evicted_L1;
            VC.set_heap[0].blocks[open_block].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);;
            VC.set_heap[0].blocks[open_block].valid = true;
            VC.set_heap[0].blocks[open_block].dirty = dirty_evicted_L1;

            VC.set_heap[0].set_mru(VC.set_heap[0].blocks[open_block].tag);

            return;
        }
    }

    /* have to contend for possible evicted block from victim cache or L1 if VC not enabled - write-back to L2 */
    uint64_t victim_index;
    if (addr_evicted_VC == 0) {
        victim_index = (addr_evicted_L1 >> num_offset_bits) & ((1 << L2.index_size) - 1);
    }
    else {
        victim_index = (addr_evicted_VC >> num_offset_bits) & ((1 << L2.index_size) - 1);
    }
    
    /* check to see if the L2 cache has any open spots */
    open_block = L2.set_heap[victim_index].check_availability();
    if (open_block < 0) {
        /* no open spots, evict L2 LRU to DRAm */
        uint64_t tag_LRU_L2 = L2.set_heap[victim_index].get_lru();
        
        /* find index of LRU in block vector */
        int block_LRU_L2;
        for (block_LRU_L2 = 0; block_LRU_L2 < L2.ways; block_LRU_L2++) {
            if (L2.set_heap[victim_index].blocks[block_LRU_L2].tag == tag_LRU_L2) break;
        }
        
        /* delete the LRU from the block vector and lru_stack - no need to save since it's just going to DRAM */
        L2.set_heap[victim_index].delete_block(tag_LRU_L2);

        /* save incoming block data to newly evicted spot - depending on whether the block came from L1 or VC */
        if (addr_evicted_VC == 0) {
            L2.set_heap[victim_index].blocks[block_LRU_L2].addr = addr_evicted_L1;
            L2.set_heap[victim_index].blocks[block_LRU_L2].tag = (addr_evicted_L1 >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
            L2.set_heap[victim_index].blocks[block_LRU_L2].dirty = false;
            L2.set_heap[victim_index].blocks[block_LRU_L2].valid = true;
        }
        else {
            L2.set_heap[victim_index].blocks[block_LRU_L2].addr = addr_evicted_VC;
            L2.set_heap[victim_index].blocks[block_LRU_L2].tag = (addr_evicted_VC >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
            L2.set_heap[victim_index].blocks[block_LRU_L2].dirty = false;
            L2.set_heap[victim_index].blocks[block_LRU_L2].tag = true;
        }

        stats->write_backs_l1_or_victim_cache++;

        /* set as MRU or LRU depending on L2 policy */
        if (insert_policy == INSERT_POLICY_MIP) {
            L2.set_heap[victim_index].set_mru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
        }
        else {
            L2.set_heap[victim_index].set_lru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
        }
    }
    else {
        /* open blocks found - just save incoming data to empty block and set as MRU or LRU depending on policy */
        if (addr_evicted_VC == 0) {
            L2.set_heap[victim_index].blocks[open_block].addr = addr_evicted_L1;
            L2.set_heap[victim_index].blocks[open_block].tag = (addr_evicted_L1 >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
            L2.set_heap[victim_index].blocks[open_block].dirty = false;
            L2.set_heap[victim_index].blocks[open_block].valid = true;
        }
        else {
            L2.set_heap[victim_index].blocks[open_block].addr = addr_evicted_VC;
            L2.set_heap[victim_index].blocks[open_block].tag = (addr_evicted_VC >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
            L2.set_heap[victim_index].blocks[open_block].dirty = false;
            L2.set_heap[victim_index].blocks[open_block].valid = true;
        }

        stats->write_backs_l1_or_victim_cache++;

        if (insert_policy == INSERT_POLICY_MIP) {
            L2.set_heap[victim_index].set_mru(L2.set_heap[victim_index].blocks[open_block].tag);
        }
        else {
            L2.set_heap[victim_index].set_lru(L2.set_heap[victim_index].blocks[open_block].tag);
        }

        return;
    }
}

void sim_finish(sim_stats_t *stats) {
    /* calculate stats */
    stats->hit_ratio_l1 = stats->hits_l1 / (stats->accesses_l1 + stats->misses_l1);
    stats->miss_ratio_l1 = stats->misses_l1 / (stats->accesses_l1 + stats->hits_l1);

    stats->hit_ratio_victim_cache = stats->hits_victim_cache / (stats->misses_l1 + stats->misses_victim_cache);
    stats->miss_ratio_victim_cache = stats->misses_victim_cache / (stats->misses_l1 + stats->hits_victim_cache);

    stats->read_hit_ratio_l2 = stats->read_hits_l2 / (stats->reads_l2 + stats->read_misses_l2);
    stats->read_miss_ratio_l2 = stats->read_misses_l2 / (stats->reads_l2 + stats->read_hits_l2);

    stats->avg_access_time_l2 = (L2_HIT_TIME_CONST + L2_HIT_TIME_PER_S * log2(L2.ways)) + (stats->read_miss_ratio_l2) * DRAM_ACCESS_PENALTY;
    stats->avg_access_time_l1 = (L1_HIT_TIME_CONST + L1_HIT_TIME_PER_S * log2(L1.ways)) + (stats->miss_ratio_victim_cache * stats->miss_ratio_l1) * stats->avg_access_time_l2;
}

