#ifndef TAGE_H
#define TAGE_H

#include "branchsim.hpp"
#include "TAGE_GHR.hpp"
#include "Counter.hpp"

// TAGE predictor definition
class tage : public branch_predictor_base
{

private:
    
    // TODO: add your fields here


public:
    // create optional helper functions here

    void init_predictor(branchsim_conf *sim_conf);
    
    // Return the prediction ({taken/not-taken}, target-address)
    bool predict(branch *branch, branchsim_stats *sim_stats);
    
    // Update the branch predictor state
    void update_predictor(branch *branch);

    // Cleanup any allocated memory here
    ~tage();
};

#endif
