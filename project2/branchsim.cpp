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

using namespace std;

/* Save N for final stat calculations */
int n;

/* Don't modify this line -- it's to make the compiler happy */
branch_predictor_base::~branch_predictor_base() {}

/* ----------------------------------------- GSHARE BRANCH PREDICTOR -------------------------------------------- */

void gshare::shift_ghr(int new_val) {
    /* shift in new ghr value, zero out anything beyond the length of the ghr */
    ghr <<= 1;
    ghr += new_val;
    ghr <<= (64 - g);
    ghr >>= (64 - g);
}

uint64_t gshare::hash_0(uint64_t pc) {
    /* XOR ghr and pc[2 + g - 1:2] */
    bitset<64> pc_b(pc);
    bitset<64> ghr_b(ghr);
    bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < g; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t gshare::hash_1(uint64_t pc) {
    /* XOR ghr and pc[g - 1:0] */
    bitset<64> pc_b(pc);
    bitset<64> ghr_b(ghr);
    bitset<64> hash_b(0);

    for (int i = 0; i < g; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t gshare::hash_2(uint64_t pc) {
    /* add ghr and pc[2 + g - 1: 2] */
    pc >>= 2;
    uint64_t hash = pc + ghr;

    bitset<64> hash_b(hash);
    for (int i = g; i < 64; i ++) {
        hash_b[i] = 0;
    }

    return (uint64_t) hash_b.to_ullong();
}

void gshare::init_predictor(branchsim_conf *sim_conf) {
    /* set gshare parameters */
    g = log2(pow(2, sim_conf->S) / 2);
    t_size = pow(2, g);
    p = sim_conf->P;
    ghr = 0;

    /* initialize prediction counters and tags */
    counters = new Counter*[t_size];
    tags = new uint64_t[t_size];
    for (int i = 0; i < t_size; i ++) {
        counters[i] = new Counter(2);
        tags[i] = 0;
    }

    n = sim_conf->N;

    #ifdef DEBUG
        printf("Using GShare, size: %d KiB, G: %d, using hash function: %d\n", (int) (pow(2, sim_conf->S) / 8 / 1024), (int) g, (int) sim_conf->P);
    #endif
}   

bool gshare::predict(branch *branch, branchsim_stats *sim_stats) {
    /* hash instruction pointer depending on selected hash function */
    uint64_t index;
    if (p == 0) 
        index = hash_0(branch->ip);
    else if (p == 1)
        index = hash_1(branch->ip);
    else 
        index = hash_2(branch->ip);
    
    /* see if the indexed counter is taken or not */
    bool taken = counters[index]->isTaken();

    /* increment tag conflicts if the tag of the counter is not equal to the current tag */
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
    /* hash instruction pointer depending on selected hash function */
    uint64_t index;
    if (p == 0) 
        index = hash_0(branch->ip);
    else if (p == 1)
        index = hash_1(branch->ip);
    else 
        index = hash_2(branch->ip);
    
    /* update indexed counter based on actual branch outcome */
    counters[index]->update(branch->is_taken);

    /* set indexed tag to the current instruction pointer */
    tags[index] = branch->ip;

    /* shift in branch outcome to ghr */
    shift_ghr(branch->is_taken);

    #ifdef DEBUG
        printf("\tUpdating Entry: 0x%" PRIx64 ", set Tag: 0x%" PRIx64 ", new state: 0x%x, new GHR: 0x%" PRIx64 "\n", index, branch->ip, (uint) counters[index]->get(), ghr);
    #endif
}

gshare::~gshare() {
    /* free counters and tags */
    for (int i = 0; i < t_size; i++)
        delete[] counters[i];
    delete[] counters;
    delete tags;
}

/* ------------------------------------------ TAGE-S BRANCH PREDICTOR ------------------------------------------- */

int tage::history_length(int x) {
    /* recursively solve for the length of the ghr for a given table x */
    if (x > 1)
        return floor(1.75 * history_length(x - 1) + 0.5);
    else
        return floor(e / 2.0 + 0.5);
}

uint64_t tage::hash_bimodal(uint64_t pc) {
    /* return pc[2 + h - 1:2] to index into table zero */
    bitset<64> pc_b(pc);
    bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < h; i ++) {
        hash_b[i] = pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t tage::hash_tagged(uint64_t pc, int i) {
    /* XOR pc[2 + e - 1:2] and compressed history calculated with table history length */
    bitset<64> pc_b(pc);
    bitset<64> ghr_b(ghr.getCompressedHistory(history_length(i), e));
    bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < e; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t tage::get_partial(uint64_t pc) {
    /* XOR pc[2 + e + t − 1:2 + e] and ghr[t - 1: 0] */
    bitset<64> pc_b(pc);
    bitset<64> ghr_b(ghr.getHistory());
    bitset<64> hash_b(0);

    pc_b >>= (2 + e);
    for (int i = 0; i < t; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

void tage::init_predictor(branchsim_conf *sim_conf) {
    /* set tage parameters */
    h = sim_conf->S - 4;
    p = sim_conf->P;
    t = p + 4;
    e = floor(log2(((pow(2, sim_conf->S) - (2 * pow(2, h))) / (t + 3 + 2)) / p));

    /* initialize TAGE-GHR object */
    ghr = TAGE_GHR(history_length(p));

    /* calculate sizes of table zero and tagged tables */
    t0_size = pow(2, h);
    tx_size = pow(2, e);

    /* initialize table zero counters */
    table_zero = new Counter*[t0_size];
    for (int i = 0; i < t0_size; i ++)
        table_zero[i] = new Counter(2);

    /* intialize tagged table predictors, useful counters, and partial tags */
    tagged_predictors = new Counter**[p];
    tagged_useful = new Counter**[p];
    partial_tags = new uint64_t*[p];
    for (int i = 0; i < p; i ++) {
        tagged_predictors[i] = new Counter*[tx_size];
        tagged_useful[i] = new Counter*[tx_size];
        partial_tags[i] = new uint64_t[tx_size];
        for (int j = 0; j < tx_size; j ++) {
            tagged_predictors[i][j] = new Counter(3, 0);
            tagged_useful[i][j] = new Counter(2, 0);
            partial_tags[i][j] = 0;
        }
    }

    n = sim_conf->N;

    #ifdef DEBUG
        printf("Using TAGE, size: %d KiB, number of tagged tables: %d, H: %d, T: %d, E: %d, L(%d): %d\n", (int) (pow(2, sim_conf->S) / 8 / 1024), p, h, t, e, p, history_length(p));
    #endif
}

bool tage::predict(branch *branch, branchsim_stats *sim_stats) {
    bool taken;
    int longest_match = -1;
    uint64_t partial = get_partial(branch->ip);

    /* search tagged tables for a partial tag, excluding 'new' entries */
    for (int i = p - 1; i >= 0; i --) {
        uint64_t index = hash_tagged(branch->ip, i + 1);
        if (partial_tags[i][index] == partial) {
            if (!(tagged_predictors[i][index]->isWeak() && tagged_useful[i][index]->get() == 0)) {
                taken = tagged_predictors[i][index]->isTaken();
                longest_match = i + 1;

                #ifdef DEBUG
                    if (taken) 
                        printf("\tIP: 0x%" PRIx64 ", Hit in Table T%d, at index 0x%" PRIx64 ", Partial Tag: 0x%" PRIx64 ", Useful Counter: %d, Prediction: 0x%x (Taken)\n", branch->ip, i + 1, index, partial, (int) tagged_useful[i][index]->get(), (uint) tagged_predictors[i][index]->get());
                    else
                        printf("\tIP: 0x%" PRIx64 ", Hit in Table T%d, at index 0x%" PRIx64 ", Partial Tag: 0x%" PRIx64 ", Useful Counter: %d, Prediction: 0x%x (Not Taken)\n", branch->ip, i + 1, index, partial, (int) tagged_useful[i][index]->get(), (uint) tagged_predictors[i][index]->get());
                #endif

                break;
            }
        }
    }

    /* if no match, search table zero */
    if (longest_match == -1) {
        sim_stats->num_tag_conflicts ++;
        uint64_t index = hash_bimodal(branch->ip);
        taken = table_zero[index]->isTaken();
        longest_match = 0;

        #ifdef DEBUG
        if (taken)
            printf("\tIP: 0x%" PRIx64 ", Hit in Table T0, index: 0x%" PRIx64 ", prediction: 0x%x (Taken)\n", branch->ip, index, (uint) table_zero[index]->get());
        else 
            printf("\tIP: 0x%" PRIx64 ", Hit in Table T0, index: 0x%" PRIx64 ", prediction: 0x%x (Not Taken)\n", branch->ip, index, (uint) table_zero[index]->get());
        #endif
    }
    
    return taken;
}

void tage::update_predictor(branch *branch) {
    bool new_prediction;
    int longest_match = -1;
    uint64_t partial = get_partial(branch->ip);

    /* search for longest match, including any 'new' entries */
    for (int i = p - 1; i >= 0; i --) {
        uint64_t index = hash_tagged(branch->ip, i + 1);
        if (partial_tags[i][index] == partial) {
            longest_match = i + 1;
            
            /* update useful counters and predictors */
            if (tagged_predictors[i][index]->isTaken() == branch->is_taken) 
                tagged_useful[i][index]->update(true);
            else
                tagged_useful[i][index]->update(false);
            
            tagged_predictors[i][index]->update(branch->is_taken);
            new_prediction = tagged_predictors[i][index]->isTaken();

            #ifdef DEBUG
                printf("\tUpdating T%d entry: 0x%" PRIx64 ", new prediction counter: 0x%x, new useful counter: 0x%x\n", longest_match, index, (int) tagged_predictors[i][index]->get(), (int) tagged_useful[i][index]->get());
            #endif

            break;
        }
    }

    /* no longest match found, update table zero instead */
    if (longest_match == -1) {
        uint64_t index = hash_bimodal(branch->ip);
        longest_match = 0;

        table_zero[index]->update(branch->is_taken);
        new_prediction = table_zero[index]->isTaken();

        #ifdef DEBUG
            printf("\tUpdating T0 entry: 0x%" PRIx64 ", new state: 0x%x\n", index, (int) table_zero[index]->get());
        #endif
    }

    /* search for allocatable entries, if none found decrement T_x+ useful counters */
    if (new_prediction != branch->is_taken && longest_match < p) {
        bool new_allocated = false;

        for (int i = longest_match; i < p; i ++) {
            uint64_t index = hash_tagged(branch->ip, i + 1);
            if (tagged_useful[i][index]->get() == 0) {
                new_allocated = true;
                partial_tags[i][index] = partial;
                tagged_predictors[i][index]->reset(branch->is_taken);

                #ifdef DEBUG
                    printf("\tAllocating entry in table T%d, index: 0x%" PRIx64 ", new partial tag: 0x%" PRIx64 ", useful counter: 0x%x, prediction counter: 0x%x\n", i + 1, index, partial, 0, (uint) tagged_predictors[i][index]->get());
                #endif

                break;
            }
        }

        if (!new_allocated) {
            for (int i = p - 1; i >= longest_match; i --) {
                uint64_t index = hash_tagged(branch->ip, i + 1);
                tagged_useful[i][index]->update(false);
            }

            #ifdef DEBUG
                printf("\tNo available entry for allocation, decrementing useful counters for table(s) T%d+\n", longest_match + 1);
            #endif 
        }
    }

    #ifdef DEBUG
        if (longest_match == p)
            printf("\tNo available entry for allocation, No useful counters to decrement because T%d does not exist\n", longest_match + 1);
    #endif
    
    /* update ghr with branch outcome */
    ghr.shiftLeft(branch->is_taken);

    #ifdef DEBUG
        printf("\tUpdated GHR (lower bits shown): 0x%" PRIx64 "\n", ghr.getHistory());
    #endif
}

tage::~tage() {
    /* free t_0 table */
    for (int i = 0; i < t0_size; i++)
        delete[] table_zero[i];
    delete[] table_zero;

    /* free tagged tables, including their tags, predictors, and useful counters */
    for (int i = 0; i < p; i ++) {
        for (int j = 0; j < tx_size; j ++) {
            delete[] tagged_predictors[i][j];
            delete[] tagged_useful[i][j];
        }
        delete[] tagged_predictors[i];
        delete[] tagged_useful[i];
        delete partial_tags[i];
    }
    delete[] tagged_predictors;
    delete[] tagged_useful;
    delete[] partial_tags;
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
