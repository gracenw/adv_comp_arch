#include <iostream>
#include <cmath>
#include <iterator>
#include <algorithm>
#include <bitset>
#include <inttypes.h>

#include "branchsim.hpp"
#include "gshare.hpp"
#include "tage.hpp"
#include "TAGE_GHR.hpp"
#include "Counter.hpp"

/* Save N for final stat calculations */
int n;

/* Don't modify this line -- it's to make the compiler happy */
branch_predictor_base::~branch_predictor_base() {}

/* ----------------------------------------- GSHARE BRANCH PREDICTOR -------------------------------------------- */

void gshare::shift_ghr(int new_val) {
    ghr <<= 1;
    ghr += new_val;
    ghr <<= (64 - ghr_width);
    ghr >>= (64 - ghr_width);
}

uint64_t gshare::hash_0(uint64_t pc) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(ghr);
    std::bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < ghr_width; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t gshare::hash_1(uint64_t pc) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(ghr);
    std::bitset<64> hash_b(0);

    for (int i = 0; i < ghr_width; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t gshare::hash_2(uint64_t pc) {
    pc >>= 2;
    uint64_t hash = pc + ghr;

    std::bitset<64> hash_b(hash);
    for (int i = ghr_width; i < 64; i ++) {
        hash_b[i] = 0;
    }

    return (uint64_t) hash_b.to_ullong();
}

void gshare::init_predictor(branchsim_conf *sim_conf) {
    ghr_width = log2(pow(2, sim_conf->S) / 2);
    num_counters = pow(2, ghr_width);
    hash_func = sim_conf->P;
    ghr = 0;

    counters = new Counter*[num_counters];
    tags = (uint64_t *) malloc(sizeof(uint64_t) * num_counters);
    for (int i = 0; i < num_counters; i ++) {
        counters[i] = new Counter(2);
        tags[i] = 0;
    }

    n = sim_conf->N;

    #ifdef DEBUG
        printf("Using GShare, size: %d KiB, G: %d, using hash function: %d\n", (int) (pow(2, sim_conf->S) / 8 / 1024), (int) ghr_width, (int) sim_conf->P);
    #endif
}   

bool gshare::predict(branch *branch, branchsim_stats *sim_stats) {
    uint64_t index;
    if (hash_func == 0) 
        index = hash_0(branch->ip);
    else if (hash_func == 1)
        index = hash_1(branch->ip);
    else 
        index = hash_2(branch->ip);

    bool taken = counters[index]->isTaken();

    if (tags[index] != branch->ip)
        sim_stats->num_tag_conflicts ++;

    #ifdef DEBUG
        printf("\tMaking prediction for ip = 0x%" PRIx64 ", index = 0x%" PRIx64 ", tag = 0x%" PRIx64 ", GHR: 0x%" PRIx64 "\n", branch->ip, index, branch->ip, ghr);
        if (tags[index] == branch->ip && taken)
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag match, prediction: 0x%x (Taken)\n", tags[index], (uint) counters[index]->get());
        else if (tags[index] == branch->ip && !taken)
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag match, prediction: 0x%x (Not Taken)\n", tags[index], (uint) counters[index]->get());
        else if (tags[index] != branch->ip && taken)
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag mismatch, prediction: 0x%x (Taken)\n", tags[index], (uint) counters[index]->get());
        else
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag mismatch, prediction: 0x%x (Not Taken)\n", tags[index], (uint) counters[index]->get());
    #endif

    return taken;
}

void gshare::update_predictor(branch *branch) {
    uint64_t index;
    if (hash_func == 0) 
        index = hash_0(branch->ip);
    else if (hash_func == 1)
        index = hash_1(branch->ip);
    else 
        index = hash_2(branch->ip);
    
    counters[index]->update(branch->is_taken);
    tags[index] = branch->ip;
    shift_ghr(branch->is_taken);

    #ifdef DEBUG
        printf("\tUpdating Entry: 0x%" PRIx64 ", set Tag: 0x%" PRIx64 ", new state: 0x%x, new GHR: 0x%" PRIx64 "\n", index, branch->ip, (uint) counters[index]->get(), ghr);
    #endif
}

gshare::~gshare() {
    for (int i = 0; i < num_counters; i++)
        delete[] counters[i];
    delete[] counters;
    free(tags);
}

/* ------------------------------------------ TAGE-S BRANCH PREDICTOR ------------------------------------------- */

TaggedTable::TaggedTable(int i, int e, int t, int l) {
    subscript = i;
    num_entries = pow(2, e);
    tag_size = t;
    history_length = l;

    predict_counters = new Counter*[num_entries];
    useful_counters = new Counter*[num_entries];
    partial_tags = (uint64_t *) malloc(sizeof(uint64_t) * num_entries);
    for (int i = 0; i < num_entries; i ++) {
        predict_counters[i] = new Counter(3, 0);
        useful_counters[i] = new Counter(2, 0);
        partial_tags[i] = 0;
    }
}

TaggedTable::~TaggedTable() {
    for (int i = 0; i < num_entries; i ++) {
        delete[] predict_counters[i];
        delete[] useful_counters[i];
    }
    delete[] predict_counters;
    delete[] useful_counters;
    free(partial_tags);
}

int tage::history_length(int x) {
    if (x > 1)
        return floor(1.75 * history_length(x - 1) + 0.5);
    else
        return floor(e / 2.0 + 0.5);
}

uint64_t tage::hash_bimodal(uint64_t pc) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(ghr.getHistory());
    std::bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < h; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t tage::hash_tagged(uint64_t pc, int i) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(ghr.getCompressedHistory(tagged_tables[i]->history_length, e));
    std::bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < e; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t tage::get_partial(uint64_t pc) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(ghr.getHistory());
    std::bitset<64> hash_b(0);

    pc_b >>= (2 + e);
    for (int i = 0; i < e; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

void tage::init_predictor(branchsim_conf *sim_conf) {
    h = sim_conf->S - 4;
    p = sim_conf->P;
    t = p + 4;
    e = floor(log2(((pow(2, sim_conf->S) - (2 * pow(2, h))) / (t + 3 + 2)) / p));
    ghr = TAGE_GHR(history_length(p));

    int t0_size = pow(2, h);
    table_zero = new Counter*[t0_size];
    for (int i = 0; i < t0_size; i ++)
        table_zero[i] = new Counter(2);

    tagged_tables = new TaggedTable*[p];
    for (int i = 0; i <= p; i ++) 
        tagged_tables[i] = new TaggedTable(i, e, t, history_length(i));

    n = sim_conf->N;

    #ifdef DEBUG
        printf("Using TAGE, size: %d KiB, number of tagged tables: %d, H: %d, T: %d, E: %d, L(%d): %d\n", (int) (pow(2, sim_conf->S) / 8 / 1024), p, h, t, e, p, history_length(p));
    #endif
}

bool tage::predict(branch *branch, branchsim_stats *sim_stats) {
    bool taken;
    uint64_t partial = get_partial(branch->ip);
    for (int i = p - 1; i >= 0; i --) {
        uint64_t index = hash_tagged(branch->ip, i + 1);
        if (tagged_tables[i]->partial_tags[index] == partial) {
            if (!(tagged_tables[i]->predict_counters[index]->isWeak() && tagged_tables[i]->useful_counters[index]->get() == 0)) {
                taken = tagged_tables[i]->predict_counters[index]->isTaken();

                #ifdef DEBUG
                    if (taken) 
                        printf("\tIP: 0x%" PRIx64 ", Hit in Table T%d, at index 0x%" PRIx64 ", Partial Tag: 0x%" PRIx64 ", Useful Counter: %d, Prediction: 0x%x (Taken)\n", branch->ip, i + 1, index, partial, (int) tagged_tables[i]->useful_counters[index]->get(), (uint) tagged_tables[i]->predict_counters[index]->get());
                    else
                        printf("\tIP: 0x%" PRIx64 ", Hit in Table T%d, at index 0x%" PRIx64 ", Partial Tag: 0x%" PRIx64 ", Useful Counter: %d, Prediction: 0x%x (Not Taken)\n", branch->ip, i + 1, index, partial, (int) tagged_tables[i]->useful_counters[index]->get(), (uint) tagged_tables[i]->predict_counters[index]->get());
                #endif

                return taken;
            }
        }
    }

    sim_stats->num_tag_conflicts ++;
    uint64_t index = hash_bimodal(branch->ip);
    taken = table_zero[index]->isTaken();

    #ifdef DEBUG
        if (taken)
            printf("\tIP: 0x%" PRIx64 ", Hit in Table T0, index: 0x%" PRIx64 ", prediction: 0x%x (Taken)\n", branch->ip, index, (uint) table_zero[index]->get());
        else 
            printf("\tIP: 0x%" PRIx64 ", Hit in Table T0, index: 0x%" PRIx64 ", prediction: 0x%x (Not Taken)\n", branch->ip, index, (uint) table_zero[index]->get());
    #endif

    return taken;
}

void tage::update_predictor(branch *branch) {
    int longest_match_table = -1;
    bool taken;
    uint64_t partial = get_partial(branch->ip);
    for (int i = p - 1; i >= 0; i --) {
        uint64_t index = hash_tagged(branch->ip, i + 1);
        if (tagged_tables[i]->partial_tags[index] == partial) {
            if (tagged_tables[i]->predict_counters[index]->isWeak() && tagged_tables[i]->useful_counters[index]->get() == 0) {
                tagged_tables[i]->useful_counters[index]->update(false);
            }
            else {
                tagged_tables[i]->useful_counters[index]->update(true);
            }

            taken = tagged_tables[i]->predict_counters[index]->isTaken();
            tagged_tables[i]->predict_counters[index]->update(branch->is_taken);

            #ifdef DEBUG
                printf("\tUpdating T%d entry: 0x%" PRIx64 ", new prediction counter: 0x%x, new useful counter: 0x%x\n", i + 1, index, (int) tagged_tables[i]->predict_counters[index]->get(), (int) tagged_tables[i]->useful_counters[index]->get());
            #endif

            longest_match_table = i + 1;
            break;
        }
    }

    if (longest_match_table == -1) {
        uint64_t index = hash_bimodal(branch->ip);
        taken = table_zero[index]->isTaken();
        table_zero[index]->update(branch->is_taken);
        longest_match_table = 0;

        #ifdef DEBUG
            printf("\tUpdating T0 entry: 0x%" PRIx64 ", new state: 0x%x\n", index, (int) table_zero[index]->get());
        #endif
    }

    /*
        printf("\tNo avaliable entry for allocation, No useful counters to decrement because T%d does not exist\n", ...)
        printf("\tNo avaliable entry for allocation, decrementing useful counters for table(s) T%d+\n", ...)
    */

    if (taken != branch->is_taken) {
        bool new_allocated = false;
        for (int i = p - 1; i > longest_match_table; i --) {
            uint64_t index = hash_tagged(branch->ip, i + 1);
            if (tagged_tables[i]->useful_counters[index]->get() == 0) {
                tagged_tables[i]->partial_tags[index] = partial;
                tagged_tables[i]->predict_counters[index]->reset(branch->is_taken);
                new_allocated = true;

                #ifdef DEBUG
                    printf("\tAllocating entry in table T%d, index: 0x%" PRIx64 ", new partial tag: 0x%" PRIx64 ", useful counter: 0x%x, prediction counter: 0x%x\n", i + 1, index, partial, 0, (uint) tagged_tables[i]->predict_counters[index]->get());
                #endif
            }
        }

        if (!new_allocated) {
            for (int i = p - 1; i > longest_match_table; i --) {
                uint64_t index = hash_tagged(branch->ip, i + 1);
                tagged_tables[i]->useful_counters[index]->update(false);
            }
        }
    }

    #ifdef DEBUG
        if (longest_match_table == p)
            printf("\tNo avaliable entry for allocation, No useful counters to decrement because T%d does not exist\n", longest_match_table + 1);
    #endif

    ghr.shiftLeft(branch->is_taken);

    #ifdef DEBUG
        printf("\tUpdated GHR (lower bits shown): 0x%" PRIx64 "\n", std::bitset<12>(ghr.getHistory()).to_ulong());
    #endif
}

tage::~tage() {
    for (int i = 0; i < pow(2, h); i++)
        delete[] table_zero[i];
    delete[] table_zero;
    
    for (int i = 0; i < p; i ++) 
        delete[] tagged_tables[i];
    delete[] tagged_tables;
}

/* --------------------- Common Functions to update statistics and final computations, etc. --------------------- */

/**
 *  Function to update the branchsim stats per prediction. Here we now know the
 *  actual outcome of the branch, so you can update misprediction counters etc.
 *
 *  @param prediction The prediction returned from the predictor's predict function
 *  @param branch Pointer to the branch that is being predicted -- contains actual behavior
 *  @param stats Pointer to the simulation statistics -- update in this function
 */
void branchsim_update_stats(bool prediction, branch *branch, branchsim_stats *sim_stats) {
    sim_stats->total_instructions = branch->inst_num;
    sim_stats->num_branch_instructions ++;

    if (prediction == branch->is_taken)
        sim_stats->num_branches_correctly_predicted ++;
    else
        sim_stats->num_branches_mispredicted ++;
}

/**
 *  Function to finish branchsim statistic computations such as prediction rate, etc.
 *
 *  @param stats Pointer to the simulation statistics -- update in this function
 */
void branchsim_finish_stats(branchsim_stats *sim_stats) {
    sim_stats->misses_per_kilo_instructions = (uint64_t) ((double) sim_stats->num_branches_mispredicted / sim_stats->total_instructions * 1000);
    sim_stats->fraction_branch_instructions = (double) sim_stats->num_branch_instructions / sim_stats->total_instructions;
    sim_stats->prediction_accuracy = (double) sim_stats->num_branches_correctly_predicted / sim_stats->num_branch_instructions;
    sim_stats->N = n;

    if (sim_stats->N <= 7)
        sim_stats->stalls_per_mispredicted_branch = 2;
    else 
        sim_stats->stalls_per_mispredicted_branch = (sim_stats->N / 2) - 1;
    
    sim_stats->average_CPI = 1 + ((double) sim_stats->num_branches_mispredicted * sim_stats->stalls_per_mispredicted_branch / sim_stats->total_instructions);
}
