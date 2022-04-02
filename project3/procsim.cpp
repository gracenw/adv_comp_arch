#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <cmath>
#include <list>
#include <iterator>

#include "procsim.hpp"

#define ALU 0
#define MUL 1
#define MEM 2

using namespace std;

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

/* -------------------------- CLASSES --------------------------- */

/* instruction */
class instruction {
    public:
        uint64_t pc, ls_addr;
        opcode_t opcode;
        int8_t dest, src1, src2;
        bool intrpt;

        instruction(uint64_t _pc, uint64_t _ls_addr, opcode_t _opcode, int8_t _dest, int8_t _src1, int8_t _src2, bool _intrpt) {
            pc = _pc;
            ls_addr = _ls_addr;
            opcode = _opcode;
            dest = _dest;
            src1 = _src1;
            src2 = _src2;
            intrpt = _intrpt;
        }
};

/* reservation station */
class reserv_stat {
    public:
        int8_t dest, src1, src2;
        uint64_t tagd, tag1, tag2, ls_addr;
        bool ready1, ready2;
        int8_t fu;

        reserv_stat(int8_t _dest, int8_t _src1, int8_t _src2, uint64_t _ls_addr, int8_t _fu) {
            dest = _dest;
            src1 = _src1;
            src2 = _src2;
            tagd = 0;
            tag1 = 0;
            tag2 = 0;
            ls_addr = _ls_addr;
            ready1 = false;
            ready2 = false;
            fu = _fu;
        }
};

/* result bus */
class result_bus {
    public:
        uint64_t tag;
        int8_t reg;

        result_bus() {
            tag = 0;
            reg = -1;
        }

        void flush() {
            tag = 0;
            reg = -1;
        }
};

/* register */
class reg_entry {
    public:
        uint64_t tag;
        bool ready;

        reg_entry() {
            tag = 0;
            ready = true;
        }

        void flush(uint64_t _tag) {
            tag = _tag;
            ready = true;
        }
};

/* rob entry */
class rob_entry {
    public:
        uint64_t tag, pc;
        bool ready, excpt;
        int8_t reg;

        rob_entry(uint64_t _tag, uint64_t _pc, uint64_t, bool _ready, bool _excpt, int8_t _reg) {
            tag = _tag;
            pc = _pc;
            ready = _ready;
            excpt = _excpt;
            reg = _reg;
        }
};

/* cache entry */
class cache_entry {
    public:
        uint64_t tag;
        bool valid;

        cache_entry() {
            tag = 0;
            valid = false;
        }
};

/* functional unit */
class funct_unit {
    public:
        uint8_t type;
        opcode_t opcode;
        uint64_t pipeline[MUL_STAGES]; /* only use first stage for mul and mem */
        uint64_t stalls; /* only used with loads */

        funct_unit(uint64_t _type) {
            type = _type;
            opcode = (opcode_t) NULL;
            for (int i = 0; i < MUL_STAGES; i ++) {
                pipeline[i] = 0;
            }
        }

        void flush() {
            opcode = (opcode_t) NULL;
            for (int i = 0; i < MUL_STAGES; i ++) {
                pipeline[i] = 0;
            }
            stalls = 0;
        }
};

/* --------------------------- LISTS ---------------------------- */

/* infinite dispatch queue */
list<instruction*> dispq;

/* scheduling queue */
list<reserv_stat*> schedq;

/* reorder buffer */
list<rob_entry*> rob;

/* result buses */
result_bus ** rbuses;

/* scoreboard */
funct_unit ** scoreb;

/* data cache */
cache_entry ** dcache;

/* messy register file */
reg_entry ** messy;

/* ---------------------- USEFUL VARIABLES ---------------------- */

uint64_t NUM_ALU, NUM_MUL, NUM_MEM, NUM_FUS, INDEX_SIZE, TAG_SIZE, CACHE_SETS, SCHED_SIZE, ROB_SIZE, MAX_FETCH, MAX_RETIRE;
uint64_t reserved = 0;
uint64_t curr_tag = 0;

/* ---------------------- HELPER FUNCTIONS ---------------------- */

/* fetches instructions from the icache and appends them to the dispatch queue */
static void stage_fetch(procsim_stats_t *stats) {
    for (int i = 0; i < MAX_FETCH; i ++) {
        /* fetch instruction using driver */
        const inst_t * instr = procsim_driver_read_inst();

        /* make instruction object */
        instruction * temp = new instruction(instr->pc, instr->load_store_addr, instr->opcode, instr->dest, instr->src1, instr->src2, instr->interrupt);

        /* add the back of dispatch queue */
        dispq.push_back(temp);

        /* delete temporary object */
        delete temp;
    }
}

/* looks through dispatch queue, decodes instructions, and inserts into the scheduling queue */
static void stage_dispatch(procsim_stats_t *stats) {
    /*
    Dispatch will use the tags and ready bits from the messy register file to set up the reservation station
    in the scheduling queue. Dispatch also allocates a new tag for each instruction regardless of whether
    it has a destination register. This is done to maintain program order in the ROB and across the
    processor where necessary. The dispatch stage will allocate room in the ROB for the instruction as it places it into a
    reservation station.
    */

    /* make sure there is space in the scheduling queue and rob */
    if (schedq.size() < SCHED_SIZE && (rob.size() + reserved) < ROB_SIZE) {
        instruction * temp = dispq.front();
        dispq.pop_front();
        reserved ++;
    }
}

/* looks through scheduling queue and fires instructions */
static void stage_schedule(procsim_stats_t *stats) {
    /*
        If multiple instructions can be fired at the same time, they
        are fired in program order (but still within the same cycle). This means that if 2 instructions are
        both ready for a single function unit, the instruction that comes first in program order would fire
        first. Instructions stay in the SQ until they retire.
        Load/store can fire only if no Load or Store instruction in the scheduling queue that both has an
        index conflict in the cache and comes earlier in program order (tag order)
    */

}

/* moves instructions through pipelined FUs; removes instruction from scehd queue when it has completed */
static void stage_exec(procsim_stats_t *stats) {
    /* iterate through the function units - first is alu */
    for (int i = 0; i < NUM_ALU; i ++) {
        
    }

    /* mul function units */
    for (int i = NUM_ALU; i < (NUM_ALU + NUM_MUL); i ++) {
        
    }

    /* mem function units */
    for (int i = (NUM_ALU + NUM_MUL); i < NUM_FUS; i ++) {
        if (scoreb[i]->opcode == OPCODE_LOAD) {
            uint64_t addr = scoreb[i]->pipeline[0];
            uint64_t tag = (addr >> (INDEX_SIZE + DCACHE_B)) & ((1 << TAG_SIZE) - 1);
            uint64_t index = (addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);

            if (scoreb[i]->stalls > 0) {
                if (scoreb[i]->stalls == L2_LATENCY_CYCLES) {
                    /* set fu to not busy */
                    scoreb[i]->stalls = 0;
                    scoreb[i]->pipeline[0] = 0;

                    /* bring L2 data to cache */
                    dcache[index]->tag = tag;
                    dcache[index]->valid = true;

                    /* remove instruction from sched q and put in rob */


                    /* update ready bits in messy and schedq */

                }
                else {
                    /* still waiting on L2 */
                    scoreb[i]->stalls ++;
                }
            }
            else {
                if (tag != dcache[index]->tag || !dcache[index]->valid) {
                    /* cache miss */
                    scoreb[i]->stalls ++; 
                }
            }
        }
    }

    // when instructions are in the final pipeline stage of an FU and aren't stalled, set ready bits in the messy & schedq
    
    
    
}

/* pops instructions from head of ROB and returns number of instructions retired */
static uint64_t stage_state_update(procsim_stats_t *stats, bool *retired_interrupt_out) {
    uint64_t num_retired = 0;

    /* iterate through rob for length of retire width */
    for (int i = 0; i < MAX_RETIRE; i ++) {
        if (rob.size() > 0) {
            rob_entry * front = rob.front();
            rob.pop_front();

            /* interrupt found, set retired interrupt flag to true and return */
            if (front->excpt) {
                *retired_interrupt_out = true;
                num_retired ++;
                return num_retired;
            }
        }
    }

    /* no interrupts, retired max width */
    *retired_interrupt_out = false;
    return num_retired;
}

/* resets all states on an interrupt, sets all messy regs to ready */
static void flush_pipeline(void) {
    /* erase all entries in dispatch queue, scheduling queue, and rob */
    dispq.clear();
    schedq.clear();
    rob.clear();

    /* erase reserved slots */
    reserved = 0;

    /* flush and reset all function units and result buses */
    for (int i = 0; i < NUM_FUS; i ++) {
        /* dcache is left as is - but pending loads not fulfilled */
        scoreb[i]->flush();
        rbuses[i]->flush();
    }

    /* set all messy regs to ready */
    for (int i = 0; i < NUM_REGS; i ++) {
        messy[i]->flush(curr_tag); // FIX ME??
    }
}

/* ----------------------- SIM FUNCTIONS ------------------------ */

/* initialize data structures, simulator state, and statistics */
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    /* initalize useful simulator variables */
    NUM_ALU    = sim_conf->num_alu_fus;
    NUM_MUL    = sim_conf->num_mul_fus;
    NUM_MEM    = sim_conf->num_lsu_fus;
    NUM_FUS    = NUM_ALU + NUM_MUL + NUM_MEM;
    INDEX_SIZE = sim_conf->dcache_c - DCACHE_B;
    TAG_SIZE   = 64 - (INDEX_SIZE + DCACHE_B);
    CACHE_SETS = pow(2, sim_conf->dcache_c) / pow(2, DCACHE_B);
    SCHED_SIZE = NUM_FUS * sim_conf->num_schedq_entries_per_fu;
    ROB_SIZE   = sim_conf->num_rob_entries;
    MAX_FETCH  = sim_conf->fetch_width;
    MAX_RETIRE = sim_conf->retire_width;

    /* initialize result buses to empty */
    rbuses = new result_bus*[NUM_FUS];
    for (int i = 0; i < NUM_FUS; i ++) {
        rbuses[i] = new result_bus();
    }

    /* initialize scoreboard to all fus not busy */
    scoreb = new funct_unit*[NUM_FUS];
    for (int i = 0; i < NUM_ALU; i ++) {
        scoreb[i] = new funct_unit(ALU);
    }
    for (int i = NUM_ALU; i < (NUM_ALU + NUM_MUL); i ++) {
        scoreb[i] = new funct_unit(MUL);
    }
    for (int i = (NUM_ALU + NUM_MUL); i < NUM_FUS; i ++) {
        scoreb[i] = new funct_unit(MEM);
    }

    /* set all dcache entries to empty and invalid */
    dcache = new cache_entry*[CACHE_SETS];
    for (int i = 0; i < CACHE_SETS; i ++) {
        dcache[i] = new cache_entry();
    }

    /* set all registers in messy to ready and empty */
    messy = new reg_entry*[NUM_REGS];
    for (int i = 0; i < NUM_REGS; i ++) {
        messy[i] = new reg_entry();
    }

    #ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", 0UL); // TODO: fix me
    printf("Initial messy RF state:\n");
    print_messy_rf();
    printf("\n");
    #endif
}
 
/* calls the stage functions above in reverse order */
uint64_t procsim_do_cycle(procsim_stats_t *stats, bool *retired_interrupt_out) {
    #ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles);
    #endif

    /* retired_interrupt_out is set by stage_state_update() */
    uint64_t retired_this_cycle = stage_state_update(stats, retired_interrupt_out);

    if (*retired_interrupt_out) {
        #ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired interrupt, so flushing pipeline!\n", retired_this_cycle);
        #endif

        /* after retiring an interrupt, flush pipeline immediately and pick up where sim left off in the next cycle */
        stats->interrupts++;
        flush_pipeline();
    } else {
        #ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire interrupt, so proceeding with other pipeline stages.\n", retired_this_cycle);
        #endif

        /* if interrupt was not retired, continue simulating the other pipeline stages */
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

    /* increment max_usages and avg_usages in stats TODO */
    stats->cycles++;

    /* return the number of instructions retired this cycle (including the interrupt if there was one) */
    return retired_this_cycle;
}

/* free allocated memory and calculate some final statistics */
void procsim_finish(procsim_stats_t *stats) {
    /* free allocated memory */
    for (int i = 0; i < NUM_FUS; i ++) {
        delete [] rbuses[i];
        delete [] scoreb[i];
    }
    for (int i = 0; i < CACHE_SETS; i ++) {
        delete [] dcache[i];
    }
    for (int i = 0; i < NUM_REGS; i ++) {
        delete [] messy[i];
    }
    delete [] rbuses;
    delete [] scoreb;
    delete [] dcache;
    delete [] messy;

    /* calculate final statistics */

}
