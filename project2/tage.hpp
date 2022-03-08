#ifndef TAGE_H
#define TAGE_H

#include "branchsim.hpp"
#include "TAGE_GHR.hpp"
#include "Counter.hpp"

/* Tagged Table for TAGE-S class definition */
class TaggedTable {
    
    public:
        int subscript, num_entries, tag_size, history_length;
        Counter ** predict_counters;
        Counter ** useful_counters;
        uint64_t * partial_tags;

        TaggedTable(int i, int e, int t, int l);
        ~TaggedTable();
};

/* TAGE-S class predictor definition */
class tage : public branch_predictor_base {

    private:
        int h, t, e, p;
        TAGE_GHR ghr;
        Counter ** table_zero;
        TaggedTable ** tagged_tables;

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

        /* Frees any allocated memory -- includes both new and malloc */
        ~tage();
};

#endif
