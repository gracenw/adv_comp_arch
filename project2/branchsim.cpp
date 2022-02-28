#include <iostream>
#include <cmath>
#include <iterator>
#include <algorithm>

#include "branchsim.hpp"
#include "gshare.hpp"
#include "tage.hpp"
#include "TAGE_GHR.hpp"
#include "Counter.hpp"

// Don't modify this line -- Its to make the compiler happy
branch_predictor_base::~branch_predictor_base() {}


// ******* Student Code starts here *******

// Gshare Branch Predictor

void gshare::init_predictor(branchsim_conf *sim_conf)
{
    // TODO: implement me
}

bool gshare::predict(branch *branch, branchsim_stats *sim_stats)
{
    // TODO: implement me
    return false;
}

void gshare::update_predictor(branch *branch)
{
    // TODO: implement me
}

gshare::~gshare()
{
    // TODO: implement me
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
    // TODO: implement me
}

/**
 *  Function to finish branchsim statistic computations such as prediction rate, etc.
 *
 *  @param stats Pointer to the simulation statistics -- update in this function
*/
void branchsim_finish_stats(branchsim_stats *sim_stats) {
    // TODO: implement me
}
