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

int pipeline_stages;

// Don't modify this line -- It's to make the compiler happy
branch_predictor_base::~branch_predictor_base() {}

// Gshare Branch Predictor

void gshare::shift_ghr(int new_val) {
    this->ghr <<= 1;
    this->ghr += new_val;
    this->ghr <<= (64 - this->ghr_width);
    this->ghr >>= (64 - this->ghr_width);
}

uint64_t gshare::hash_0(uint64_t pc) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(this->ghr);
    std::bitset<64> hash_b(0);

    pc_b >>= 2;
    for (int i = 0; i < this->ghr_width; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t gshare::hash_1(uint64_t pc) {
    std::bitset<64> pc_b(pc);
    std::bitset<64> ghr_b(this->ghr);
    std::bitset<64> hash_b(0);

    for (int i = 0; i < this->ghr_width; i ++) {
        hash_b[i] = ghr_b[i] ^ pc_b[i];
    }

    return (uint64_t) hash_b.to_ullong();
}

uint64_t gshare::hash_2(uint64_t pc) {
    pc >>= 2;
    uint64_t hash = pc + this->ghr;

    std::bitset<64> hash_b(hash);
    for (int i = this->ghr_width; i < 64; i ++) {
        hash_b[i] = 0;
    }

    return (uint64_t) hash_b.to_ullong();
}

void gshare::init_predictor(branchsim_conf *sim_conf) {
    this->ghr_width = log2(pow(2, sim_conf->S) / 2); /* 12 to 18 bits wide */
    this->num_counters = pow(2, this->ghr_width); /* 2^G counters, equal to total size / 2 */
    this->hash_func = sim_conf->P;
    this->ghr = 0;

    this->counters = new Counter*[this->num_counters];
    this->tags = (uint64_t *) malloc(sizeof(uint64_t) * this->num_counters);
    for (int i = 0; i < this->num_counters; i ++) {
        counters[i] = new Counter(2);
        tags[i] = 0;
    }

    pipeline_stages = sim_conf->N;

    #ifdef DEBUG
        printf("Using GShare, size: %d KiB, G: %d, using hash function: %d\n", (int) (pow(2, sim_conf->S) / 8 / 1024), (int) this->ghr_width, (int) sim_conf->P);
    #endif
}   

bool gshare::predict(branch *branch, branchsim_stats *sim_stats) {
    uint64_t index;
    if (this->hash_func == 0) 
        index = this->hash_0(branch->ip);
    else if (this->hash_func == 1)
        index = this->hash_1(branch->ip);
    else 
        index = this->hash_2(branch->ip);

    bool taken = this->counters[index]->isTaken();

    if (this->tags[index] != branch->ip)
        sim_stats->num_tag_conflicts ++;

    #ifdef DEBUG
        printf("\tMaking prediction for ip = 0x%" PRIx64 ", index = 0x%" PRIx64 ", tag = 0x%" PRIx64 ", GHR: 0x%" PRIx64 "\n", branch->ip, index, branch->ip, this->ghr);
        if (this->tags[index] == branch->ip && taken)
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag match, prediction: 0x%x (Taken)\n", this->tags[index], (uint) this->counters[index]->get());
        else if (this->tags[index] == branch->ip && !taken)
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag match, prediction: 0x%x (Not Taken)\n", this->tags[index], (uint) this->counters[index]->get());
        else if (this->tags[index] != branch->ip && taken)
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag mismatch, prediction: 0x%x (Taken)\n", this->tags[index], (uint) this->counters[index]->get());
        else
            printf("\tFound entry with tag: 0x%" PRIx64 ", Tag mismatch, prediction: 0x%x (Not Taken)\n", this->tags[index], (uint) this->counters[index]->get());
    #endif

    return taken;
}

void gshare::update_predictor(branch *branch) {

    uint64_t index;
    if (this->hash_func == 0) 
        index = this->hash_0(branch->ip);
    else if (this->hash_func == 1)
        index = this->hash_1(branch->ip);
    else 
        index = this->hash_2(branch->ip);
    
    this->counters[index]->update(branch->is_taken);
    tags[index] = branch->ip;
    this->shift_ghr(branch->is_taken);

    #ifdef DEBUG
        printf("\tUpdating Entry: 0x%" PRIx64 ", set Tag: 0x%" PRIx64 ", new state: 0x%x, new GHR: 0x%" PRIx64 "\n", index, branch->ip, (uint) this->counters[index]->get(), this->ghr);
    #endif
}

gshare::~gshare() {
    for (int i = 0; i < this->num_counters; i++)
        delete[] this->counters[i];
    delete[] this->counters;
    free(this->tags);
}

// TAGE PREDICTOR

void tage::init_predictor(branchsim_conf *sim_conf) {
    // TODO: implement me
}

bool tage::predict(branch *branch, branchsim_stats *sim_stats)
{
    // TODO: implement me
    return false;
}

void tage::update_predictor(branch *branch) {
    // TODO: implement me
}

tage::~tage() {
    // TODO: implement me
}

// Common Functions to update statistics and final computations, etc.

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
    sim_stats->N = pipeline_stages;

    if (sim_stats->N <= 7)
        sim_stats->stalls_per_mispredicted_branch = 2;
    else 
        sim_stats->stalls_per_mispredicted_branch = (sim_stats->N / 2) - 1;
    
    sim_stats->average_CPI = 1 + ((double) sim_stats->num_branches_mispredicted * sim_stats->stalls_per_mispredicted_branch / sim_stats->total_instructions);
}
