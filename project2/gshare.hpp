#ifndef GHSARE_H
#define GSHARE_H

#include "branchsim.hpp"
#include "Counter.hpp"

// Gshare predictor definition
class gshare : public branch_predictor_base
{
    private:
        int ghr_width, num_counters, hash_func;
        uint64_t ghr;
        Counter ** counters;
        uint64_t * tags;

    public:
        void shift_ghr(int new_val);

        uint64_t hash_0(uint64_t pc);
        uint64_t hash_1(uint64_t pc);
        uint64_t hash_2(uint64_t pc);

        void init_predictor(branchsim_conf *sim_conf);

        // Return the prediction
        bool predict(branch *branch, branchsim_stats *sim_stats);

        // Update the branch predictor state
        void update_predictor(branch *branch);

        // Cleanup any allocated memory here
        ~gshare();
};

#endif
