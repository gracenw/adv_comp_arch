#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "procsim.hpp"

// The helper functions in this #ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)
#ifdef DEBUG
static void print_operand(int8_t rx) {
    if (rx < 0) {
        printf("(none)");
    } else {
        printf("R%" PRId8, rx);
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction(const inst_t *inst) {
    static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    printf("opcode=%s, dest=", opcode_names[inst->opcode]);
    print_operand(inst->dest);
    printf(", src1=");
    print_operand(inst->src1);
    printf(", src2=");
    print_operand(inst->src2);
}

static void print_messy_rf(void) {
    for (uint64_t regno = 0; regno < NUM_REGS; regno++) {
        if (regno == 0) {
            printf("    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, 0UL, 0); // TODO: fix me
        } else if (!(regno & 0x3)) {
            printf("\n    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, 0UL, 0); // TODO: fix me
        } else {
            printf(", R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, 0UL, 0); // TODO: fix me
        }
    }
    printf("\n");
}

static void print_schedq(void) {
    size_t printed_idx = 0;
    for (/* ??? */; /* ??? */ false; /* ??? */) { // TODO: fix me
        if (printed_idx == 0) {
            printf("    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", 0, 0UL, 0UL, 0, 0UL, 0); // TODO: fix me
        } else if (!(printed_idx & 0x1)) {
            printf("\n    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", 0, 0UL, 0UL, 0, 0UL, 0); // TODO: fix me
        } else {
            printf(", {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", 0, 0UL, 0UL, 0, 0UL, 0); // TODO: fix me
        }

        printed_idx++;
    }
    if (!printed_idx) {
        printf("    (scheduling queue empty)");
    }
    printf("\n");
}

static void print_rob(void) {
    size_t printed_idx = 0;
    printf("    Next tag to retire (head of ROB): %" PRIu64 "\n", 0UL); // TODO: fix me
    for (/* ??? */; /* ??? */ false; /* ??? */) {
        if (printed_idx == 0) {
            printf("    { tag: %" PRIu64 ", interrupt: %d }", 0UL, 0); // TODO: fix me
        } else if (!(printed_idx & 0x3)) {
            printf("\n    { tag: %" PRIu64 ", interrupt: %d }", 0UL, 0); // TODO: fix me
        } else {
            printf(", { tag: %" PRIu64 " interrupt: %d }", 0UL, 0); // TODO: fix me
        }

        printed_idx++;
    }
    if (!printed_idx) {
        printf("    (ROB empty)");
    }
    printf("\n");
}
#endif

//
// TODO: Define any useful data structures and functions here
//

// Optional helper function which resets all state on an interrupt, including
// the dispatch queue, scheduling queue, the ROB, etc. Note the dcache should
// NOT be flushed here! The messy register file needs to have all register set
// to ready (and in a real system, we would copy over the values from the
// architectural register file; however, this is a simulation with no values to
// copy!)
static void flush_pipeline(void) {
    // TODO: fill me in
}

// Optional helper function which pops instructions from the head of the ROB
// with a maximum of retire_width instructions retired. (In a real system, the
// destination register value from the ROB would be written to the
// architectural register file, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring an interrupt, this function will set
// *retired_interrupt_out = true and return immediately. Note that in this
// case, the interrupt must be counted as one of the retired instructions.
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_interrupt_out) {
    // TODO: fill me in
    *retired_interrupt_out = false;
    return 0;
}

// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the messy register file and
// scheduling queue. This function should remove an instruction from the
// scheduling queue when it has completed.
static void stage_exec(procsim_stats_t *stats) {
    // TODO: fill me in
}

// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions. Note that when multiple
// instructions are ready to fire in a given cycle, they must be fired in
// program order. Also, load and store instructions must be fired according to
// the modified TSO ordering described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats) {
    // TODO: fill me in
}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB; effectively, dispatch allocates
// reservation stations and ROB space for each instruction dispatched and
// stalls if there is either is unavailable. Note the scheduling queue and ROB
// have configurable sizes. The PDF has details.
static void stage_dispatch(procsim_stats_t *stats) {
    // TODO: fill me in
}

// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. To simplify the
// project, the dispatch queue is infinite in size.
static void stage_fetch(procsim_stats_t *stats) {
    // TODO: fill me in
}

// Use this function to initialize all your data structures, simulator
// state, and statistics.
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    // TODO: fill me in

    #ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", 0UL); // TODO: fix me
    printf("Initial messy RF state:\n");
    print_messy_rf();
    printf("\n");
    #endif
}

// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if an interrupt was retired by assigning true or false to
// *retired_interrupt_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_interrupt_out) {
    #ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles);
    #endif

    // stage_state_update() should set *retired_interrupt_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_interrupt_out);

    if (*retired_interrupt_out) {
        #ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired interrupt, so flushing pipeline!\n", retired_this_cycle);
        #endif

        // After we retire an interrupt, flush the pipeline immediately and
        // then pick up where we left off in the next cycle.
        stats->interrupts++;
        flush_pipeline();
    } else {
        #ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire interrupt, so proceeding with other pipeline stages.\n", retired_this_cycle);
        #endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

    #ifdef DEBUG
    printf("End-of-cycle dispatch queue usage: %lu\n", 0UL); // TODO: fix me
    printf("End-of-cycle messy RF state:\n");
    print_messy_rf();
    printf("End-of-cycle scheduling queue state:\n");
    print_schedq();
    printf("End-of-cycle ROB state:\n");
    print_rob();
    printf("================================ End cycle %" PRIu64 " ================================\n", stats->cycles);
    #endif

    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;

    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats) {
    // TODO: fill me in
}
