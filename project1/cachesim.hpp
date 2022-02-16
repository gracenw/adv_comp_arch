#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h> 
#include <vector>
#include <iostream>
#include <bitset>

// Combined insertion and replacement policy
typedef enum insert_policy {
    // Standard insertion policy used in LRU replacement where new blocks are
    // set to the MRU position
    INSERT_POLICY_MIP,
    // LIP inserts blocks at the LRU position (instead of MRU) as proposed by
    // Qureshi et al. (2007). Please see the PDF for details
    INSERT_POLICY_LIP,
} insert_policy_t;

typedef enum write_strat {
    // Write back, write-allocate
    WRITE_STRAT_WBWA,
    // Write through, write-no-allocate
    WRITE_STRAT_WTWNA,
} write_strat_t;

typedef struct cache_config {
    bool disabled;
    // (C,B,S) in the Conte Cache Taxonomy (Patent Pending)
    uint64_t c;
    uint64_t b;
    uint64_t s;
    insert_policy_t insert_policy;
    write_strat_t write_strat;
} cache_config_t;

typedef struct sim_config {
    cache_config_t l1_config;
    uint64_t victim_cache_entries;
    cache_config_t l2_config;
} sim_config_t;

typedef struct sim_stats {
    uint64_t reads;
    uint64_t writes;
    uint64_t accesses_l1;
    uint64_t reads_l2;
    uint64_t writes_l2;
    uint64_t write_backs_l1_or_victim_cache;
    uint64_t hits_l1;
    uint64_t hits_victim_cache;
    uint64_t read_hits_l2;
    uint64_t misses_l1;
    uint64_t misses_victim_cache;
    uint64_t read_misses_l2;
    double hit_ratio_l1;
    double hit_ratio_victim_cache;
    double read_hit_ratio_l2;
    double miss_ratio_l1;
    double miss_ratio_victim_cache;
    double read_miss_ratio_l2;
    double avg_access_time_l1;
    double avg_access_time_l2;
} sim_stats_t;

extern void sim_setup(sim_config_t *config);
extern void sim_access(char rw, uint64_t addr, sim_stats_t* p_stats);
extern void sim_finish(sim_stats_t *p_stats);

// Sorry about the /* comments */. C++11 cannot handle basic C99 syntax,
// unfortunately
static const sim_config_t DEFAULT_SIM_CONFIG = {
    /*.l1_config =*/ {/*.disabled =*/ 0,
                      /*.c =*/ 10, // 1KB Cache
                      /*.b =*/ 6,  // 64-byte blocks
                      /*.s =*/ 1,  // 2-way
                      /*.insert_policy =*/ INSERT_POLICY_MIP,
                      /*.write_strat =*/ WRITE_STRAT_WBWA},

    /*.victim_cache_entries =*/ 2,

    /*.l2_config =*/ {/*.disabled =*/ 0,
                      /*.c =*/ 15, // 32KB Cache
                      /*.b =*/ 6,  // 64-byte blocks
                      /*.s =*/ 3,  // 8-way
                      /*.insert_policy =*/ INSERT_POLICY_LIP,
                      /*.write_strat =*/ WRITE_STRAT_WTWNA}
};

// Argument to cache_access rw. Indicates a load
static const char READ = 'R';
// Argument to cache_access rw. Indicates a store
static const char WRITE = 'W';

// This is the reciprocal of epsilon from the paper
static const uint64_t BIP_COUNTER_RESET_INTERVAL = 32;

static const double DRAM_ACCESS_PENALTY = 80;
// Hit time (HT) for a given cache (L1 or L2)
// is HIT_TIME_CONST + (HIT_TIME_PER_S * S)
static const double L1_HIT_TIME_CONST = 2;
static const double L1_HIT_TIME_PER_S = 0.2;
static const double L2_HIT_TIME_CONST = 8;
static const double L2_HIT_TIME_PER_S = 0.8;

/* STUDENT ADD ONS */
static const int addr_size = 64;

struct block {
    uint64_t tag, addr;
    bool valid, dirty;

    block(): tag(0), addr(0), valid(false), dirty(false) {}
    // block(uint64_t _tag, uint64_t _addr, bool _valid, bool _dirty): tag(_tag), addr(_addr), valid(_valid), dirty(_dirty) {}
};

struct set {
    std::vector<uint64_t> lru_stack;
    std::vector<block> blocks;

    set() {}
};

#endif /* CACHESIM_HPP */
