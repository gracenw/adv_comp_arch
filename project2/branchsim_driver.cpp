/**
 * @file branchsim_driver.cpp
 * @brief Trace reader and driver for a branch prediction project assigned in
 * CS{4/6}290 and ECE{4/6}100 for the Spring 2022 semester
 *
 * DON"T MODIFY ANY CODE IN THIS FILE!
 *
 * @author Anirudh Jain and Pulkit Gupta
 */

#include <getopt.h>
#include <unistd.h>


#include <cstdarg>
#include <cinttypes>
#include <cstdio>
#include <cstdbool>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "branchsim.hpp"
#include "gshare.hpp"
#include "tage.hpp"

// Print message and exit from the application
static inline void print_error_exit(const char *msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    fprintf(stderr, msg, argptr);
    va_end(argptr);
    exit(EXIT_FAILURE);
}


// Print error usage
static void print_err_usage(const char *err)
{

    fprintf(stderr, "%s\n", err);
    fprintf(stderr, "./branchsim -i <trace file> [Options]\n");
    fprintf(stderr, "-O [Predictor {1:Gshare, 2:TAGE-Simple}\n");
    fprintf(stderr, "-S [Size of Predictor in bits  = 2^S]\n");
    fprintf(stderr, "-P [Associated parameter for predictor]\n");
    fprintf(stderr, "-N [Num stages in modelled pipeline\n");
    fprintf(stderr, "-H <Print this message>\n");

    exit(EXIT_FAILURE);
}

// Function to print the run configuration
static void print_sim_config(branchsim_conf *sim_conf)
{
    printf("SIMULATION CONFIGURATION\n");
    printf("S:          %" PRIu64 "\n", sim_conf->S);
    printf("P:          %" PRIu64 "\n", sim_conf->P);
    printf("N:          %" PRIu64 "\n", sim_conf->N);
    printf("Predictor:  %s\n", pred_to_string.at(sim_conf->predictor));
}

// Function to print the simulation output
static void print_sim_output(branchsim_stats *sim_stats)
{
    printf("\nSIMULATION OUTPUT\n");
    printf("Total Instructions:                 %" PRIu64 "\n", sim_stats->total_instructions);
    printf("Total Branch Instructions:          %" PRIu64 "\n", sim_stats->num_branch_instructions);
    printf("Branches Correctly Predicted:       %" PRIu64 "\n", sim_stats->num_branches_correctly_predicted);
    printf("Branches Mispredicted:              %" PRIu64 "\n", sim_stats->num_branches_mispredicted);
    printf("Misses Per Kilo Instructions:       %" PRIu64 "\n", sim_stats->misses_per_kilo_instructions);
    printf("Number of Tag conflicts             %" PRIu64 "\n", sim_stats->num_tag_conflicts);

    printf("Fraction Branch Instructions:       %.8f\n", sim_stats->fraction_branch_instructions);
    printf("Branch Prediction accuracy:         %.8f\n", sim_stats->prediction_accuracy);

    printf("Total Stages in the Pipeline:       %" PRIu64 "\n", sim_stats->N);
    printf("Branch Misprediction Stalls:        %" PRIu64 "\n", sim_stats->stalls_per_mispredicted_branch);
    printf("Average CPI:                        %.8f\n", sim_stats->average_CPI);
}


// Main function for the simulator driver
int main(int argc, char *const argv[])
{
    // Trace file
    FILE *trace = NULL;

    // FILE *temp = fopen("temp.trace", "w+");

    branchsim_stats sim_stats;
    memset(&sim_stats, 0, sizeof(branchsim_stats));

    branchsim_conf sim_conf;

    int opt;
    int pred_num = -1;
    while (-1 != (opt = getopt(argc, argv, "o:O:s:S:n:N:p:P:i:I:hH"))) {
        switch (opt) {
            case 'o':
            case 'O':
                pred_num = std::atoi(optarg);
                if (pred_num < 1 || pred_num > 2) { print_err_usage("Invalid predictor option"); }
                sim_conf.predictor = (branchsim_conf::PREDICTOR) pred_num;
                break;

            case 's':
            case 'S':
                sim_conf.S = (uint64_t) std::atoi(optarg);
                break;

            case 'p':
            case 'P':
                sim_conf.P = (uint64_t) std::atoi(optarg);
                break;

            case 'n':
            case 'N':
                sim_conf.N = (uint64_t) std::atoi(optarg);
                break;

            case 'i':
            case 'I':
                trace = std::fopen(optarg, "r");
                if (trace == NULL) {
                    print_err_usage("Could not open the input trace file");
                }
                break;

            case 'h':
            case 'H':
                print_err_usage("");
                break;
            default:
                print_err_usage("Invalid argument to program");
                break;
        }
    }

    if (!trace) {
        print_err_usage("No trace file provided!");
    }

    print_sim_config(&sim_conf);

    if (pred_num == branchsim_conf::PREDICTOR::GSHARE && sim_conf.P > 2) {
        printf("invalid hash selector (P) for GShare\n");
        exit(EXIT_FAILURE);
    }

    if (pred_num == branchsim_conf::PREDICTOR::TAGE && (sim_conf.P < 3 || sim_conf.P > 10)) {
        printf("invalid number of tables (P) for TAGE\n");
        exit(EXIT_FAILURE);
    }


    printf("SETUP COMPLETE - STARTING SIMULATION\n");

    // Allocate the predictor
    branch_predictor_base *predictor;
    switch (sim_conf.predictor) {

    case branchsim_conf::TAGE:
        predictor = new tage();
        break;

    case branchsim_conf::GSHARE:
    default:
        predictor = new gshare();
        break;
    }

    // Initialize the predictor
    predictor->init_predictor(&sim_conf);

    branch branch;
    size_t num_branches = 0;
    while (!feof(trace)) {
        int is_taken;
        int ret = std::fscanf(trace, "%" PRIx64 " %d %" PRIu64 "\n", &branch.ip, &is_taken, &branch.inst_num);

        // Read branch from the input trace
        if (ret == 3) {
            branch.is_taken = is_taken;

            #ifdef DEBUG
                printf("Branch: %" PRIu64 "\n", (num_branches + 1));
            #endif

            bool prediction = predictor->predict(&branch, &sim_stats);
            branchsim_update_stats(prediction, &branch, &sim_stats);
            predictor->update_predictor(&branch);

            num_branches++;
        }
    }

    // Function cleanup final statistics
    sim_stats.N = sim_conf.N;
    branchsim_finish_stats(&sim_stats);

    // This calls the destructor of the predictor before freeing the allocated memory
    delete predictor;

    fclose(trace);

    print_sim_output(&sim_stats);

    return 0;
}
