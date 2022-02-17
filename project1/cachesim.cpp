#include "cachesim.hpp"

#include <math.h>
#include <stdlib.h> 
#include <vector>
#include <iostream>

using namespace std;

class Block {
    public:
        Block(): tag(0), addr(0), valid(false), dirty(false) {}

        uint64_t tag;
        uint64_t addr;
        bool valid;
        bool dirty;

        void clear_block() {
            this->tag = 0;
            this->addr = 0;
            this->valid = false;
            this->dirty = false;
        }
};

class Set {
    public:
        Set(int _length): length(_length) {
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

void Set::set_mru(uint64_t tag) {
    bool in_stack = false;
    for (int i = 0; i < this->length; i++) { 
        if (tag == this->lru_stack[i]) in_stack = true;
    }

    if (in_stack) {
        vector<uint64_t> temp(this->length - 1);
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
    }
    else {
        this->lru_stack[this->length - 1] = tag;
        this->set_mru(tag);
    }
}

void Set::set_lru(uint64_t tag) {
    bool in_stack = false;
    for (int i = 0; i < this->length; i++) { 
        if (tag == this->lru_stack[i]) in_stack = true;
    }

    if (in_stack) {
        vector<uint64_t> temp(this->length - 1);
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
    }
    else {
        this->lru_stack[this->length - 1] = tag;
        this->set_lru(tag);
    }
}

uint64_t Set::get_lru() {
    int i;
    for (i = 0; i < this->length; i++) {
        if (this->lru_stack[i] == 0) break;
    }
    return this->lru_stack[i - 1];
}

int Set::check_availability() {
    for (int i = 0; i < this->length; i++) {
        if (!this->blocks[i].valid) return i;
    }
    return -1;
}

int Set::search_for_hit(uint64_t tag) {
    for (int i = 0; i < this->length; i++) {
        if (this->blocks[i].tag == tag && this->blocks[i].valid) {
            return i;
        }
    }
    return -1;
}

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

        void initialize_cache() {
            for (int i = 0; i < this->sets; i++) {
                this->set_heap.push_back(Set(this->ways));
            }
        }
};

Cache L1 = Cache();
Cache L2 = Cache();
Cache VC = Cache();

static const int addr_size = 64;
int block_size, num_offset_bits;
insert_policy_t insert_policy;

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

void sim_access(char rw, uint64_t addr, sim_stats_t* stats) {
    cout << addr << endl;
    stats->accesses_l1++;

    uint64_t tag_L1 = (addr >> (L1.index_size + num_offset_bits)) & ((1 << L1.tag_size) - 1);
    uint64_t index_L1 = (addr >> num_offset_bits) & ((1 << L1.index_size) - 1);

    int hit_L1 = L1.set_heap[index_L1].search_for_hit(tag_L1);

    if (hit_L1 > 0) {
        stats->hits_l1++;

        if (rw == WRITE) {
            L1.set_heap[index_L1].blocks[hit_L1].dirty = true;
            stats->writes++;
        }
        else stats->reads++;

        L1.set_heap[index_L1].set_mru(tag_L1);
        return;
    }

    stats->misses_l1++;
    
    uint64_t tag_VC;
    if (!VC.disabled) {
        tag_VC = (addr >> num_offset_bits) & ((1 << VC.tag_size) - 1);
        
        int hit_VC = VC.set_heap[0].search_for_hit(tag_VC);

        if (hit_VC > 0) {
            stats->hits_victim_cache++;

            if (rw == WRITE) {
                VC.set_heap[0].blocks[hit_VC].dirty = true;
                stats->writes++;
            }
            else stats->reads++;

            int open_block = L1.set_heap[index_L1].check_availability();
            if (open_block < 0) {
                uint64_t tag_LRU_L1 = L1.set_heap[index_L1].get_lru();
 
                for (int i = 0; i < L1.ways; i++) {
                    if (L1.set_heap[index_L1].blocks[i].tag == tag_LRU_L1 && L1.set_heap[index_L1].blocks[i].valid) {
                        bool temp_dirty = VC.set_heap[0].blocks[hit_VC].dirty;

                        VC.set_heap[0].blocks[hit_VC].dirty = L1.set_heap[index_L1].blocks[i].dirty;
                        VC.set_heap[0].blocks[hit_VC].tag = (L1.set_heap[index_L1].blocks[i].addr >> num_offset_bits) & ((1 << VC.tag_size) - 1);
                        VC.set_heap[0].blocks[hit_VC].valid = true;
                        VC.set_heap[0].blocks[hit_VC].addr = L1.set_heap[index_L1].blocks[i].addr;

                        L1.set_heap[index_L1].blocks[i].dirty = temp_dirty;
                        L1.set_heap[index_L1].blocks[i].valid = true;
                        L1.set_heap[index_L1].blocks[i].tag = tag_L1;
                        L1.set_heap[index_L1].blocks[i].addr = addr;

                        VC.set_heap[0].set_mru(VC.set_heap[0].blocks[hit_VC].tag);
                        L1.set_heap[index_L1].set_mru(tag_L1);

                        break;
                    }
                }

                return;
            }
            else {
                L1.set_heap[index_L1].blocks[open_block].tag = tag_L1;
                L1.set_heap[index_L1].blocks[open_block].addr = addr;
                L1.set_heap[index_L1].blocks[open_block].dirty = VC.set_heap[0].blocks[hit_VC].dirty;
                L1.set_heap[index_L1].blocks[open_block].valid = true;

                VC.set_heap[0].delete_block(tag_VC);

                L1.set_heap[index_L1].set_mru(tag_L1);
            }
        }
    }

    stats->misses_victim_cache++;

    uint64_t tag_L2, index_L2;
    if (!L2.disabled) {
        if (rw == WRITE) stats->writes_l2++;
        else stats->reads_l2++;

        tag_L2 = (addr >> (L2.index_size + num_offset_bits)) & ((1 << L2.tag_size) - 1);
        index_L2 = (addr >> num_offset_bits) & ((1 << L2.index_size) - 1);

        int hit_L2 = L2.set_heap[index_L2].search_for_hit(tag_L2);

        if (hit_L2 > 0) {
            if (rw == WRITE) stats->writes++;
            else {
                stats->reads++;
                stats->read_hits_l2++;
            }

            L2.set_heap[index_L2].delete_block(tag_L2);

            uint64_t addr_evicted_L1;
            bool dirty_evicted_L1;

            int open_block = L2.set_heap[index_L2].check_availability();
            if (open_block < 0) {
                uint64_t tag_LRU_L1 = L1.set_heap[index_L1].get_lru();

                int block_LRU_L1;
                for (block_LRU_L1 = 0; block_LRU_L1 < L1.ways; block_LRU_L1++) {
                    if (L1.set_heap[index_L1].blocks[block_LRU_L1].tag == tag_LRU_L1) break;
                }

                addr_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].addr;
                dirty_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].dirty;

                L1.set_heap[index_L1].delete_block(tag_LRU_L1);

                L1.set_heap[index_L1].blocks[block_LRU_L1].addr = addr;
                L1.set_heap[index_L1].blocks[block_LRU_L1].tag = tag_L1;
                L1.set_heap[index_L1].blocks[block_LRU_L1].valid = true;
                L1.set_heap[index_L1].blocks[block_LRU_L1].dirty = false;
                
                L1.set_heap[index_L1].set_mru(tag_L1);
            }
            else {
                L1.set_heap[index_L1].blocks[open_block].addr = addr;
                L1.set_heap[index_L1].blocks[open_block].tag = tag_L1;
                L1.set_heap[index_L1].blocks[open_block].valid = true;
                L1.set_heap[index_L1].blocks[open_block].dirty = false;

                L1.set_heap[index_L1].set_mru(tag_L1);

                return;
            }            

            uint64_t addr_evicted_VC;

            if (!VC.disabled) {
                int open_block = VC.set_heap[0].check_availability();

                if (open_block < 0) {
                    uint64_t tag_LRU_VC = VC.set_heap[0].get_lru();

                    int block_LRU_VC;
                    for (block_LRU_VC = 0; block_LRU_VC < VC.ways; block_LRU_VC++) {
                        if (VC.set_heap[0].blocks[block_LRU_VC].tag == tag_LRU_VC) break;
                    }

                    addr_evicted_VC = VC.set_heap[0].blocks[block_LRU_VC].addr;

                    VC.set_heap[0].delete_block(tag_LRU_VC);

                    VC.set_heap[0].blocks[block_LRU_VC].addr = addr_evicted_L1;
                    VC.set_heap[0].blocks[block_LRU_VC].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);
                    VC.set_heap[0].blocks[block_LRU_VC].dirty = dirty_evicted_L1;
                    VC.set_heap[0].blocks[block_LRU_VC].valid = true;

                    VC.set_heap[0].set_mru( VC.set_heap[0].blocks[block_LRU_VC].tag);
                }
                else {
                    VC.set_heap[0].blocks[open_block].addr = addr_evicted_L1;
                    VC.set_heap[0].blocks[open_block].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);;
                    VC.set_heap[0].blocks[open_block].valid = true;
                    VC.set_heap[0].blocks[open_block].dirty = dirty_evicted_L1;

                    VC.set_heap[0].set_mru(VC.set_heap[0].blocks[open_block].tag);

                    return;
                }
            }

            uint64_t victim_index;
            if (addr_evicted_VC == 0) {
                victim_index = (addr_evicted_L1 >> num_offset_bits) & ((1 << L2.index_size) - 1);
            }
            else {
                victim_index = (addr_evicted_VC >> num_offset_bits) & ((1 << L2.index_size) - 1);
            }
            
            open_block = L2.set_heap[victim_index].check_availability();

            if (open_block < 0) {
                uint64_t tag_LRU_L2 = L2.set_heap[victim_index].get_lru();

                int block_LRU_L2;
                for (block_LRU_L2 = 0; block_LRU_L2 < L2.ways; block_LRU_L2++) {
                    if (L2.set_heap[victim_index].blocks[block_LRU_L2].tag == tag_LRU_L2) break;
                }

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

                L2.set_heap[victim_index].delete_block(tag_LRU_L2);

                if (insert_policy == INSERT_POLICY_MIP) {
                    L2.set_heap[victim_index].set_mru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
                }
                else {
                    L2.set_heap[victim_index].set_lru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
                }
            }
            else {
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

    uint64_t addr_evicted_L1;
    bool dirty_evicted_L1;

    int open_block = L1.set_heap[index_L1].check_availability();
    if (open_block < 0) {
        uint64_t tag_LRU_L1 = L1.set_heap[index_L1].get_lru();

        int block_LRU_L1;
        for (block_LRU_L1 = 0; block_LRU_L1 < L1.ways; block_LRU_L1++) {
            if (L1.set_heap[index_L1].blocks[block_LRU_L1].tag == tag_LRU_L1) break;
        }

        addr_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].addr;
        dirty_evicted_L1 = L1.set_heap[index_L1].blocks[block_LRU_L1].dirty;

        L1.set_heap[index_L1].blocks[block_LRU_L1].addr = addr;
        L1.set_heap[index_L1].blocks[block_LRU_L1].tag = tag_L1;
        L1.set_heap[index_L1].blocks[block_LRU_L1].valid = true;
        L1.set_heap[index_L1].blocks[block_LRU_L1].dirty = false;

        L1.set_heap[index_L1].delete_block(tag_LRU_L1);
        L1.set_heap[index_L1].set_mru(tag_L1);
    }
    else {
        L1.set_heap[index_L1].blocks[open_block].tag = tag_L1;
        L1.set_heap[index_L1].blocks[open_block].addr = addr;
        L1.set_heap[index_L1].blocks[open_block].valid = true;
        L1.set_heap[index_L1].blocks[open_block].dirty = false;

        L1.set_heap[index_L1].set_mru(tag_L1);

        return;
    }

    uint64_t addr_evicted_VC;

    if (!VC.disabled) {
        int open_block = VC.set_heap[0].check_availability();
        if (open_block < 0) {
            uint64_t tag_LRU_VC = VC.set_heap[0].get_lru();

            int block_LRU_VC;
            for (block_LRU_VC = 0; block_LRU_VC < VC.ways; block_LRU_VC++) {
                if (VC.set_heap[0].blocks[block_LRU_VC].tag == tag_LRU_VC) break;
            }

            addr_evicted_VC = VC.set_heap[0].blocks[block_LRU_VC].addr;

            VC.set_heap[0].blocks[block_LRU_VC].addr = addr_evicted_L1;
            VC.set_heap[0].blocks[block_LRU_VC].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);
            VC.set_heap[0].blocks[block_LRU_VC].dirty = dirty_evicted_L1;
            VC.set_heap[0].blocks[block_LRU_VC].valid = true;

            VC.set_heap[0].delete_block(tag_LRU_VC);
            
            VC.set_heap[0].set_mru(VC.set_heap[0].blocks[block_LRU_VC].tag);
        }
        else {
            VC.set_heap[0].blocks[open_block].addr = addr_evicted_L1;
            VC.set_heap[0].blocks[open_block].tag = (addr_evicted_L1 >> num_offset_bits) & ((1 << VC.tag_size) - 1);;
            VC.set_heap[0].blocks[open_block].valid = true;
            VC.set_heap[0].blocks[open_block].dirty = dirty_evicted_L1;

            VC.set_heap[0].set_mru(VC.set_heap[0].blocks[open_block].tag);

            return;
        }
    }

    uint64_t victim_index;
    if (addr_evicted_VC == 0) {
        victim_index = (addr_evicted_L1 >> num_offset_bits) & ((1 << L2.index_size) - 1);
    }
    else {
        victim_index = (addr_evicted_VC >> num_offset_bits) & ((1 << L2.index_size) - 1);
    }
    
    open_block = L2.set_heap[victim_index].check_availability();
    if (open_block < 0) {
        uint64_t tag_LRU_L2 = L2.set_heap[victim_index].get_lru();

        int block_LRU_L2;
        for (block_LRU_L2 = 0; block_LRU_L2 < L2.ways; block_LRU_L2++) {
            if (L2.set_heap[victim_index].blocks[block_LRU_L2].tag == tag_LRU_L2) break;
        }
        
        L2.set_heap[victim_index].delete_block(tag_LRU_L2);

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

        if (insert_policy == INSERT_POLICY_MIP) {
            L2.set_heap[victim_index].set_mru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
        }
        else {
            L2.set_heap[victim_index].set_lru(L2.set_heap[victim_index].blocks[block_LRU_L2].tag);
        }
    }
    else {
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
    // stats->avg_access_time_l1 = ;
    // stats->hit_ratio_l1 = stats->hits_l1 / stats->accesses_l1;
    // stats->miss_ratio_l1 = stats->misses_l1 / stats->accesses_l1;

    // stats->hit_ratio_victim_cache = ;
    // stats->miss_ratio_victim_cache = ;

    // stats->avg_access_time_l2 = ;
    // stats->read_hit_ratio_l2 = ;
    // stats->read_miss_ratio_l2 = ;
}

