#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <cmath>
#include <list>
#include <iostream>
#include <iterator>
#include <vector>
#include <iomanip>
#include <cstring>

#include "procsim.hpp"

#define ALU 0
#define MUL 1
#define MEM 2

using namespace std;

/* ---------------------- USEFUL VARIABLES ---------------------- */

uint64_t NUM_ALU, NUM_MUL, NUM_MEM, NUM_FUS, INDEX_SIZE, TAG_SIZE, CACHE_SETS, SCHED_SIZE, ROB_SIZE, MAX_FETCH, MAX_RETIRE;
uint64_t curr_tag = 0;

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
        int8_t dest;
        uint64_t tagd, tag1, tag2, ls_addr;
        bool ready1, ready2, fired;
        opcode_t opcode;

        reserv_stat(int8_t _dest, uint64_t _ls_addr, opcode_t _opcode) {
            dest = _dest;
            tagd = 0;
            tag1 = 0;
            tag2 = 0;
            ls_addr = _ls_addr;
            ready1 = false;
            ready2 = false;
            fired = false;
            opcode = _opcode;
        }
};

/* result bus */
class result_bus {
    public:
        uint64_t tag;
        int8_t reg;
        bool busy;

        result_bus() {
            tag = 0;
            reg = -1;
            busy = false;
        }

        void flush() {
            tag = 0;
            reg = -1;
            busy = false;
        }
};

/* register */
class reg_entry {
    public:
        uint64_t tag;
        bool ready;

        reg_entry() {
            tag = curr_tag;
            curr_tag ++;
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

        rob_entry(uint64_t _tag, uint64_t _pc, bool _ready, bool _excpt, int8_t _reg) {
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
        uint8_t type, l2_stall_cycles; /* l2_stall_cycles only used by load */
        opcode_t opcode;
        bool busy, l2_stall; /* l2_stall only used by loads */
        int8_t dregs[MUL_STAGES + 1]; /* only use first stage for mul and mem */
        uint64_t dtags[MUL_STAGES + 1]; /* only use first stage for mul and mem */
        uint64_t ls_addr; /* only used with loads */

        funct_unit(uint64_t _type) {
            type = _type;
            opcode = (opcode_t) NULL;
            busy = false;
            l2_stall = false;
            for (uint64_t i = 0; i < (MUL_STAGES + 1); i ++) {
                dregs[i] = -1;
                dtags[i] = 0;
            }
            l2_stall_cycles = 0;
            ls_addr = 0;
        }

        void flush() {
            opcode = (opcode_t) NULL;
            busy = false;
            l2_stall = false;
            for (uint64_t i = 0; i < (MUL_STAGES + 1); i ++) {
                dregs[i] = -1;
                dtags[i] = 0;
            }
            l2_stall_cycles = 0;
            ls_addr = 0;
        }
};

/* --------------------------- LISTS ---------------------------- */

/* infinite dispatch queue */
list<instruction> dispq;

/* scheduling queue */
list<reserv_stat> schedq;

/* reorder buffer */
list<rob_entry> rob;

/* scoreboard */
vector<funct_unit> scoreb;

/* result buses */
vector<result_bus> rbuses;

/* data cache */
vector<cache_entry> dcache;

/* messy register file */
vector<reg_entry> messy;

/* ---------------------- DEBUG FUNCTIONS ----------------------- */

#ifdef DEBUG
static void print_operand(int8_t rx) {
    if (rx < 0) {
        printf("(none)");
    } else {
        printf("R%" PRId8, rx);
    }
}

static void print_instruction(instruction &inst) {
    static const char * opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};
    printf("opcode=%s, dest=", opcode_names[inst.opcode]);
    print_operand(inst.dest);
    printf(", src1=");
    print_operand(inst.src1);
    printf(", src2=");
    print_operand(inst.src2);
}

static void print_messy_rf(void) {
    for (uint64_t regno = 0; regno < NUM_REGS; regno++) {
        if (regno == 0) {
            printf("    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy[regno].tag, messy[regno].ready); 
        } else if (!(regno & 0x3)) {
            printf("\n    R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy[regno].tag, messy[regno].ready);
        } else {
            printf(", R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", regno, messy[regno].tag, messy[regno].ready);
        }
    }
    printf("\n");
}

static void print_schedq(void) {
    size_t printed_idx = 0;
    list<reserv_stat>::iterator it;
    for (it = schedq.begin(); it != schedq.end(); it ++) {
        if (printed_idx == 0) {
            printf("    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", (*it).fired, (*it).tagd, (*it).tag1, (*it).ready1, (*it).tag2, (*it).ready2);
        } else if (!(printed_idx & 0x1)) {
            printf("\n    {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", (*it).fired, (*it).tagd, (*it).tag1, (*it).ready1, (*it).tag2, (*it).ready2);
        } else {
            printf(", {fired: %d, tag: %" PRIu64 ", src1_tag: %" PRIu64 " (ready=%d), src2_tag: %" PRIu64 " (ready=%d)}", (*it).fired, (*it).tagd, (*it).tag1, (*it).ready1, (*it).tag2, (*it).ready2);
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
    uint64_t next_tag = rob.front().tag;
    if (rob.size() == 0) {
        next_tag = curr_tag;
    }
    printf("    Next tag to retire (head of ROB): %" PRIu64 "\n", next_tag);
    list<rob_entry>::iterator it;
    for (it = rob.begin(); it != rob.end(); it ++) {
        if ((*it).ready) {
            if (printed_idx == 0) {
                printf("    { tag: %" PRIu64 ", interrupt: %d }", (*it).tag, (*it).excpt);
            } else if (!(printed_idx & 0x3)) {
                printf("\n    { tag: %" PRIu64 ", interrupt: %d }", (*it).tag, (*it).excpt);
            } else {
                printf(", { tag: %" PRIu64 " interrupt: %d }", (*it).tag, (*it).excpt);
            }

            printed_idx++;
        }
    }
    if (!printed_idx) {
        printf("    (ROB empty)");
    }
    printf("\n");
}
#endif

/* ---------------------- HELPER FUNCTIONS ---------------------- */

/* pops instructions from head of ROB and returns number of instructions retired */
static uint64_t stage_state_update(procsim_stats_t *stats, bool *retired_interrupt_out) {
    uint64_t num_retired = 0;

    /* iterate through rob until max retire width reached */
    list<rob_entry>::iterator it;
    for (it = rob.begin(); it != rob.end(); it ++) {
        if ((*it).ready) {
            if (num_retired < MAX_RETIRE) {
                num_retired ++;
                stats->instructions_retired ++;

                /* architectural file is written to by non-store instructions */
                if ((*it).reg != -1) {
                    stats->arf_writes ++;

                    #ifdef DEBUG
                    printf("Retiring instruction with tag %" PRIu64 ". Writing R%" PRId8 " to architectural register file\n", (*it).tag, (*it).reg);
                    #endif

                    /* interrupt found, set retired interrupt flag to true and return */
                    if ((*it).excpt) {
                        *retired_interrupt_out = true;
                        return num_retired;
                    }
                }
                else {
                    #ifdef DEBUG
                    printf("Retiring instruction with tag %" PRIu64 " (Not writing to architectural register file because no dest)\n", (*it).tag);
                    #endif

                    /* interrupt found, set retired interrupt flag to true and return */
                    if ((*it).excpt) {
                        *retired_interrupt_out = true;
                        return num_retired;
                    }
                }

                /* pop from rob */
                it = rob.erase(it);
                it --;
            }
        }
        else {
            break;
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

    /* flush and reset all function units and result buses */
    for (uint64_t i = 0; i < NUM_FUS; i ++) {
        /* dcache is left as is - but pending loads not fulfilled */
        scoreb[i].flush();
        rbuses[i].flush();
    }

    /* set all messy regs to ready */
    for (uint64_t i = 0; i < NUM_REGS; i ++) {
        messy[i].flush(curr_tag);
        curr_tag ++;
    }
}

/* moves instructions through pipelined FUs; removes instruction from sched queue when it has completed */
static void stage_exec(procsim_stats_t *stats) {
    /* iterate through the function units - first is alu */
    for (uint64_t i = 0; i < NUM_ALU; i ++) {
        /* send to result bus if it is not busy */
        if (!rbuses[i].busy && scoreb[i].busy) {
            rbuses[i].busy = true;
            rbuses[i].tag = scoreb[i].dtags[0];
            rbuses[i].reg = scoreb[i].dregs[0];

            /* set fu to not busy */
            scoreb[i].busy = false;
        }
    }
    
    /* mul function units */
    for (uint64_t i = NUM_ALU; i < (NUM_ALU + NUM_MUL); i ++) {
        /* move along regardless of result bus */
        for (int j = 3; j > 0; j --) {
            if (scoreb[i].dregs[j] == -1) {
                scoreb[i].dregs[j] = scoreb[i].dregs[j - 1];
                scoreb[i].dtags[j] = scoreb[i].dtags[j - 1];
                scoreb[i].dregs[j - 1] = -1;
                scoreb[i].dtags[j - 1] = 0;
            }
        }
        if (scoreb[i].dregs[0] == -1) {
            /* set fu to not busy */
            scoreb[i].busy = false;
        }
        if (!rbuses[i].busy && scoreb[i].dtags[3] > 0) {
            /* send to result bus if it is not busy */
            rbuses[i].busy = true;
            rbuses[i].tag = scoreb[i].dtags[3];
            rbuses[i].reg = scoreb[i].dtags[3];

            /* set fu to not busy */
            scoreb[i].busy = false;

            /* zero out completed instruction */
            scoreb[i].dregs[3] = -1;
            scoreb[i].dtags[3] = 0;
        }
    }

    /* mem function units */
    for (uint64_t i = (NUM_ALU + NUM_MUL); i < NUM_FUS; i ++) {
        if (scoreb[i].busy) {
            if (scoreb[i].opcode == OPCODE_LOAD) {
                uint64_t addr = scoreb[i].ls_addr;
                uint64_t tag = addr >> (INDEX_SIZE + DCACHE_B);
                uint64_t index = (addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);

                /* previous cache miss - check to see if data is back yet */
                if (scoreb[i].l2_stall) {
                    if (scoreb[i].l2_stall_cycles == L2_LATENCY_CYCLES) {
                        /* bring L2 data to cache */
                        dcache[index].tag = tag;
                        dcache[index].valid = true;
                        // stats->dcache_reads ++;

                        /* reset stall condition */
                        scoreb[i].l2_stall = false;
                        scoreb[i].l2_stall_cycles = 0;

                        #ifdef DEBUG
                        printf("Got response from L2, updating dcache for read from instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ")\n", scoreb[i].dtags[0], index, tag);
                        #endif
                    }

                    if (scoreb[i].l2_stall_cycles < L2_LATENCY_CYCLES) {
                        /* still waiting on L2 */
                        scoreb[i].l2_stall_cycles ++;
                    }
                }
                else { /* no previous miss - check to see if there's an immediate hit */
                    if (tag != dcache[index].tag || !dcache[index].valid) {
                        /* cache miss - set flags */
                        scoreb[i].l2_stall_cycles = 1;
                        scoreb[i].l2_stall = true;

                        /* update stats */
                        stats->dcache_read_misses ++;
                        stats->dcache_reads ++ ;

                        #ifdef DEBUG
                        printf("dcache miss for instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 "), stalling for " STRINGIFY(L2_LATENCY_CYCLES) " cycles\n", scoreb[i].dtags[0], index, tag);
                        #endif
                    }
                    else {
                        /* cache hit - update stats */
                        stats->dcache_reads ++;
                         
                        #ifdef DEBUG
                        printf("dcache hit for instruction with tag %" PRIu64 " (dcache index: %" PRIx64 ", dcache tag: %" PRIx64 ")\n", scoreb[i].dtags[0], index, tag);
                        #endif
                    }
                }

                /* now that cache has been checked, see if info can be sent on to result bus */
                if (!scoreb[i].l2_stall && !rbuses[i].busy) {
                    /* send to result bus */
                    rbuses[i].busy = true;
                    rbuses[i].reg = scoreb[i].dregs[0];
                    rbuses[i].tag = scoreb[i].dtags[0];
                    
                    /* no longer busy */
                    scoreb[i].busy = false;
                }
            }
            else {
                if (!rbuses[i].busy) {
                    /* send to result bus */
                    rbuses[i].busy = true;
                    rbuses[i].reg = scoreb[i].dregs[0];
                    rbuses[i].tag = scoreb[i].dtags[0];
                    
                    /* handle store - no longer busy */
                    scoreb[i].busy = false;
                }
            }
        }
    }
}

/* looks through scheduling queue and fires instructions */
static void stage_schedule(procsim_stats_t *stats) {
    /* update register files, scheduling queue, rob */
    for (uint64_t i = 0; i < NUM_FUS; i ++) {
        /* update messy register with result bus */
        if (rbuses[i].reg != -1 && rbuses[i].busy && messy[rbuses[i].reg].tag == rbuses[i].tag) {
            messy[rbuses[i].reg].ready = true;
            rbuses[i].busy = false;
        }

        /* update rob with result buses */
        list<rob_entry>::iterator it;
        for (it = rob.begin(); it != rob.end(); it ++) {
            if ((*it).tag == rbuses[i].tag) {
                (*it).ready = true;
                rbuses[i].busy = false;

                /* delete completed instructions from scheduling queue */
                list<reserv_stat>::iterator it;
                for (it = schedq.begin(); it != schedq.end(); it ++) {
                    if ((*it).tagd == rbuses[i].tag) {
                        #ifdef DEBUG
                        if ((*it).dest == -1) {
                            printf("Instruction with tag %" PRIu64 " and dest reg=(none) completing\n", (*it).tagd);
                        }
                        else {
                            printf("Instruction with tag %" PRIu64 " and dest reg=R%" PRId8 " completing\n", (*it).tagd, (*it).dest);
                        }
                        printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", (*it).tagd);
                        printf("Inserting instruction with tag %" PRIu64 " into the ROB\n", (*it).tagd);
                        #endif

                        it = schedq.erase(it);
                        it --;
                    }
                }
            }
        }
    }

    uint64_t num_fired = 0;

    list<reserv_stat>::iterator it;
    for (it = schedq.begin(); it != schedq.end(); it ++) {
        /* search result buses for new values to update scheduling queue */
        for (uint64_t i = 0; i < NUM_FUS; i ++) {
            if (rbuses[i].tag == (*it).tag1) {
                (*it).ready1 = true;
            }
            if (rbuses[i].tag == (*it).tag2) {
                (*it).ready2 = true;
            }
        }

        /* determine required function unit */
        uint64_t fu;
        if ((*it).opcode == OPCODE_ADD || (*it).opcode == OPCODE_BRANCH) {
            fu = ALU;
        }
        else if((*it).opcode == OPCODE_MUL) {
            fu = MUL;
        }
        else {
            fu = MEM;
        }

        /* wake up and fire if reservation station if ready */
        if ((*it).ready1 && (*it).ready2) {
            /* enfore total store ordering before checking everything */
            bool invalid_tso = false;
            if (fu == MEM) {
                uint64_t this_index = ((*it).ls_addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);
                list<reserv_stat>::iterator tso;
                for (tso = schedq.begin(); tso != it; tso ++) {
                    if ((*tso).opcode == OPCODE_LOAD || (*tso).opcode == OPCODE_STORE) {
                        uint64_t tso_index = ((*tso).ls_addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);
                        if (tso_index == this_index && (*tso).tagd < (*it).tagd) {
                            /* index conflict with instruction earlier in queue - skip rest of iteration */
                            invalid_tso = true;
                            break;
                        }
                    }
                }
            }

            if (!invalid_tso) {
                for (uint64_t i = 0; i < NUM_FUS; i ++) {
                    /* find open functional unit */
                    if (scoreb[i].type == fu && !scoreb[i].busy && !(*it).fired) {
                        /* fire instruction */
                        scoreb[i].busy = true;
                        scoreb[i].dregs[0] = (*it).dest;
                        scoreb[i].dtags[0] = (*it).tagd;
                        scoreb[i].ls_addr = (*it).ls_addr;
                        scoreb[i].opcode = (*it).opcode;

                        num_fired ++;

                        (*it).fired = true;

                        #ifdef DEBUG
                        printf("Firing instruction with tag %" PRIu64 "!\n", (*it).tagd);
                        #endif

                        break;
                    }
                }
            }
        }
    }

    if (num_fired == 0) {
        /* increment number of cycles where no instructions fired */
        stats->no_fire_cycles ++;
    }
}

/* looks through dispatch queue, decodes instructions, and inserts into the scheduling queue */
static void stage_dispatch(procsim_stats_t *stats) {

    /* make sure there is space in the scheduling queue and rob */
    while (schedq.size() < SCHED_SIZE && rob.size() < ROB_SIZE && dispq.size() > 0) {
        /* get instruction from dispq */
        instruction instr = dispq.front();

        /* update ready values and tags of sources */
        reserv_stat res_stat = reserv_stat(instr.dest, instr.ls_addr, instr.opcode);
        if (instr.src1 != -1) {
            res_stat.ready1 = messy[instr.src1].ready;
            res_stat.tag1 = messy[instr.src1].tag;
        }
        else {
            res_stat.ready1 = true;
        }
        if (instr.src2 != -1) {
            res_stat.ready2 = messy[instr.src2].ready;
            res_stat.tag2 = messy[instr.src2].tag;
        }
        else {
            res_stat.ready2 = true;
        }
        
        #ifdef DEBUG
        printf("Dispatching instruction ");
        print_instruction(instr);
        if (instr.src1 == -1 && instr.src2 == -1) {
            printf(". Assigning tag=%" PRIu64 " and setting src1_ready=1 and src2_ready=1\n", curr_tag);
        }
        else if (instr.src1 != -1 && instr.src2 == -1) {
            printf(". Assigning tag=%" PRIu64 " and setting src1_tag=%" PRIu64 ", src1_ready=%d, and src2_ready=%d\n", curr_tag, res_stat.tag1, res_stat.ready1, res_stat.ready2);
        }
        else if (instr.src1 == -1 && instr.src2 != -1) {
            printf(". Assigning tag=%" PRIu64 " and setting src1_ready=%d, src2_tag=%" PRIu64 ", and src2_ready=%d\n", curr_tag, res_stat.ready1, res_stat.tag2, res_stat.ready2);
        }
        else {
            printf(". Assigning tag=%" PRIu64 " and setting src1_tag=%" PRIu64 ", src1_ready=%d, src2_tag=%" PRIu64 ", and src2_ready=%d\n", curr_tag, res_stat.tag1, res_stat.ready1, res_stat.tag2, res_stat.ready2);
        }
        #endif

        /* give destination register unique tag and set not ready */
        res_stat.tagd = curr_tag;
        if (res_stat.dest != -1) {
            messy[res_stat.dest].tag = curr_tag;
            messy[res_stat.dest].ready = false;

            #ifdef DEBUG
            printf("Marking ready=0 and assigning tag=%" PRIu64 " for R%" PRId8 " in messy RF\n", curr_tag, res_stat.dest);
            #endif
        }
        
        /* add to back of scheduling queue */
        schedq.push_back(res_stat);

        /* add incomplete instruction to rob */
        rob.emplace_back(curr_tag, instr.pc, false, instr.intrpt, res_stat.dest);

        /* pop instruction from dispq */
        dispq.pop_front();

        /* increment tag */
        curr_tag ++;
    }

    if (rob.size() == ROB_SIZE && schedq.size() < SCHED_SIZE && dispq.size() > 0) {
        /* increment number of cycles with no dispatches bc rob is full */
        stats->rob_stall_cycles ++;
    }
}

bool end_fetch = false;

/* fetches instructions from the icache and appends them to the dispatch queue */
static void stage_fetch(procsim_stats_t *stats) {
    for (uint64_t i = 0; i < MAX_FETCH; i ++) {
        /* fetch instruction using driver */
        const inst_t * instr = procsim_driver_read_inst();
        if (instr == NULL) {
            end_fetch = true;
            break;
        }
        else {
            end_fetch = false;
        }
        
        /* create temp instruction */
        instruction temp = instruction(instr->pc, instr->load_store_addr, instr->opcode, instr->dest, instr->src1, instr->src2, instr->interrupt);

        /* add the back of dispatch queue */
        dispq.push_back(temp);

        /* increment stats */
        stats->instructions_fetched ++;

        #ifdef DEBUG 
        printf("Fetching instruction ");
        print_instruction(temp);
        printf(". Adding to dispatch queue\n");
        #endif
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
    for (uint64_t i = 0; i < NUM_FUS; i ++) {
        rbuses.emplace_back();
    }

    /* initialize scoreboard to all fus not busy */
    for (uint64_t i = 0; i < NUM_ALU; i ++) {
        scoreb.emplace_back(ALU);
    }
    for (uint64_t i = NUM_ALU; i < (NUM_ALU + NUM_MUL); i ++) {
        scoreb.emplace_back(MUL);
    }
    for (uint64_t i = (NUM_ALU + NUM_MUL); i < NUM_FUS; i ++) {
        scoreb.emplace_back(MEM);
    }

    /* set all dcache entries to empty and invalid */
    for (uint64_t i = 0; i < CACHE_SETS; i ++) {
        dcache.emplace_back();
    }

    /* set all registers in messy to ready and empty */
    for (uint64_t i = 0; i < NUM_REGS; i ++) {
        messy.emplace_back();
    }

    #ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", SCHED_SIZE);
    printf("Initial messy RF state:\n");
    print_messy_rf();
    printf("\n");
    #endif
}
 
/* calls the stage functions above in reverse order */
uint64_t procsim_do_cycle(procsim_stats_t *stats, bool *retired_interrupt_out, bool *end_simulation) {
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
        if (!end_fetch || retired_interrupt_out) {
            stage_fetch(stats);
        }
        else {
            if (rob.size() == 0) {
                *end_simulation = true;
            }
        }
    }

    #ifdef DEBUG
    printf("End-of-cycle dispatch queue usage: %lu\n", dispq.size());
    printf("End-of-cycle messy RF state:\n");
    print_messy_rf();
    printf("End-of-cycle scheduling queue state:\n");
    print_schedq();
    printf("End-of-cycle ROB state:\n");
    print_rob();
    printf("================================ End cycle %" PRIu64 " ================================\n\n", stats->cycles);
    #endif

    /* increment number of cycles */
    stats->cycles++;

    /* adjust max usage metrics */
    if (stats->dispq_max_usage < dispq.size()) {
        stats->dispq_max_usage = dispq.size();
    }
    if (stats->schedq_max_usage < schedq.size()) {
        stats->schedq_max_usage = schedq.size();
    }
    if (stats->rob_max_usage < rob.size()) {
        stats->rob_max_usage = rob.size();
    }

    /* increment avg usages measurements (avg calculated at end of sim) */
    stats->dispq_avg_usage += dispq.size();
    stats->schedq_avg_usage += schedq.size();

    uint64_t rob_ready = 0;
    list<rob_entry>::iterator it;
    for (it = rob.begin(); it != rob.end(); it ++) {
        if ((*it).ready) {
            rob_ready ++;
        }
    }
    stats->rob_avg_usage += rob_ready;

    /* return the number of instructions retired this cycle (including the interrupt if there was one) */
    return retired_this_cycle;
}

/* calculate some final statistics */
void procsim_finish(procsim_stats_t *stats) {
    /* calculate final average use statistics */
    stats->dispq_avg_usage = (double) stats->dispq_avg_usage / (double) stats->cycles;
    stats->schedq_avg_usage = (double) stats->schedq_avg_usage / (double) stats->cycles;
    stats->rob_avg_usage = (double) stats->rob_avg_usage / (double) stats->cycles;

    /* calculate final cache statistics */
    stats->dcache_read_miss_ratio = (double) stats->dcache_read_misses / (double) stats->dcache_reads;
    stats->dcache_read_aat = 1 + (double) stats->dcache_read_miss_ratio * (double) L2_LATENCY_CYCLES;

    /* calculate final ipc */
    stats->ipc = (double) stats->instructions_retired / (double) stats->cycles;
}
