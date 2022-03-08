#ifndef GHSARE_H
#define GSHARE_H

#include "branchsim.hpp"
#include "Counter.hpp"

/* GSHARE class predictor definition */
class gshare : public branch_predictor_base
{
    private:
        int ghr_width, num_counters, hash_func;
        uint64_t ghr;
        Counter ** counters;
        uint64_t * tags;

    public:
        /* Shifts in new value to the GHR */
        void shift_ghr(int new_val);

        /* Hash functions for indexing into Smith counter table */
        uint64_t hash_0(uint64_t pc);
        uint64_t hash_1(uint64_t pc);
        uint64_t hash_2(uint64_t pc);
        
        /* Initializes GSHARE predictor */
        void init_predictor(branchsim_conf *sim_conf);

        /* Returns predicted value for the given instruction */
        bool predict(branch *branch, branchsim_stats *sim_stats);

        /* Updates GSHARE branch predictor state */
        void update_predictor(branch *branch);

        /* Frees any allocated memory -- includes both new and malloc */
        ~gshare();
};

#endif
