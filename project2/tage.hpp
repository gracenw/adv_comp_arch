#ifndef TAGE_H
#define TAGE_H

#include "branchsim.hpp"
#include "TAGE_GHR.hpp"
#include "Counter.hpp"

/* TAGE-S class predictor definition */
class tage : public branch_predictor_base {

    private:
        int h, t, e, p, t0_size, tx_size;
        TAGE_GHR ghr;
        Counter ** table_zero;
        Counter *** tagged_predictors;
        Counter *** tagged_useful;
        uint64_t ** partial_tags;

    public:
        /* Returns the length of the history for a given table */
        int history_length(int x);

        /* Hash function to index into T0 predictor table */
        uint64_t hash_bimodal(uint64_t pc);

        /* Hash function to index into tagged table */
        uint64_t hash_tagged(uint64_t pc, int i);

        /* Returns the partial tag for the given instruction */
        uint64_t get_partial(uint64_t pc);

        /* Initializes TAGE-S predictor */
        void init_predictor(branchsim_conf *sim_conf);
        
        /* Returns predicted value for the given instruction */
        bool predict(branch *branch, branchsim_stats *sim_stats);
        
        /* Updates TAGE-S branch predictor state */
        void update_predictor(branch *branch);

        /* Frees any allocated memory */
        ~tage();
};

#endif
