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
        bool ready1, ready2;
        opcode_t opcode;

        reserv_stat(int8_t _dest, uint64_t _ls_addr, opcode_t _opcode) {
            dest = _dest;
            tagd = 0;
            tag1 = 0;
            tag2 = 0;
            ls_addr = _ls_addr;
            ready1 = false;
            ready2 = false;
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
        uint8_t type;
        opcode_t opcode;
        bool busy;
        int8_t dregs[MUL_STAGES]; /* only use first stage for mul and mem */
        uint64_t dtags[MUL_STAGES]; /* only use first stage for mul and mem */
        uint64_t stalls, ls_addr; /* only used with loads */

        funct_unit(uint64_t _type) {
            type = _type;
            opcode = (opcode_t) NULL;
            busy = false;
            for (uint64_t i = 0; i < MUL_STAGES; i ++) {
                dregs[i] = -1;
                dtags[i] = 0;
            }
            stalls = 0;
            ls_addr = 0;
        }

        void flush() {
            opcode = (opcode_t) NULL;
            busy = false;
            for (uint64_t i = 0; i < MUL_STAGES; i ++) {
                dregs[i] = -1;
                dtags[i] = 0;
            }
            stalls = 0;
            ls_addr = 0;
        }
};

/* --------------------------- LISTS ---------------------------- */

/* infinite dispatch queue */
list<instruction*> dispq;

/* scheduling queue */
list<reserv_stat*> schedq;

/* reorder buffer */
list<rob_entry*> rob;

/* scoreboard */
funct_unit ** scoreb;

/* result buses */
result_bus ** rbuses;

/* data cache */
cache_entry ** dcache;

/* messy register file */
reg_entry ** messy;

/* ---------------------- HELPER FUNCTIONS ---------------------- */

/* fetches instructions from the icache and appends them to the dispatch queue */
static void stage_fetch(procsim_stats_t *stats) {
    for (uint64_t i = 0; i < MAX_FETCH; i ++) {
        /* fetch instruction using driver */
        const inst_t * instr = procsim_driver_read_inst();

        /* make instruction object */
        instruction * temp = new instruction(instr->pc, instr->load_store_addr, instr->opcode, instr->dest, instr->src1, instr->src2, instr->interrupt);

        /* add the back of dispatch queue */
        dispq.push_back(temp);

        /* delete temporary object */
        delete temp;

        /* increment stats */
        stats->instructions_fetched ++;
    }
}

/* looks through dispatch queue, decodes instructions, and inserts into the scheduling queue */
static void stage_dispatch(procsim_stats_t *stats) {
    uint64_t num_dispatched = 0;

    /* make sure there is space in the scheduling queue and rob */
    while (schedq.size() < SCHED_SIZE && rob.size() < ROB_SIZE) {
        num_dispatched ++;

        /* get instruction from dispq */
        instruction * instr = dispq.front();

        /* update ready values and tags of sources */
        reserv_stat * res_stat = new reserv_stat(instr->dest, instr->ls_addr, instr->opcode);
        if (messy[instr->src1]->ready) {
            res_stat->ready1 = true;
        }
        else {
            res_stat->tag1 = messy[instr->src1]->tag;
            res_stat->ready1 = false;
        }
        if (messy[instr->src2]->ready) {
            res_stat->ready2 = true;
        }
        else {
            res_stat->tag2 = messy[instr->src2]->tag;
            res_stat->ready2 = false;
        }

        /* give destination register unique tag and set not ready */
        curr_tag ++;
        res_stat->tagd = curr_tag;
        if (res_stat->dest != -1) {
            messy[res_stat->dest]->tag = curr_tag;
            messy[res_stat->dest]->ready = false;
        }
        else { /* store function - no dest register */
            messy[res_stat->dest]->ready = true;
        }
        
        /* add to back of scheduling queue */
        schedq.push_back(res_stat);

        /* add incomplete instruction to rob */
        rob_entry * rob_ntry = new rob_entry(curr_tag, instr->pc, false, instr->intrpt, res_stat->dest);
        rob.push_back(rob_ntry);
        delete res_stat;
        delete rob_ntry;

        /* pop instruction from dispq */
        dispq.pop_front();
    }

    if (num_dispatched == 0 && rob.size() == ROB_SIZE) {
        /* increment number of cycles with no dispatches bc rob is full */
        stats->rob_stall_cycles ++;
    }
}

/* looks through scheduling queue and fires instructions */
static void stage_schedule(procsim_stats_t *stats) {
    uint64_t num_fired = 0;

    list<reserv_stat*>::iterator it;
    for (it = schedq.begin(); it != schedq.end(); it ++) {
        /* search result buses for new values to update scheduling queue */
        for (uint64_t i = 0; i < NUM_FUS; i ++) {
            if (rbuses[i]->tag == (*it)->tag1) {
                (*it)->ready1 = true;
            }
            if (rbuses[i]->tag == (*it)->tag2) {
                (*it)->ready2 = true;
            }
        }

        /* determine required function unit */
        uint64_t fu;
        if ((*it)->opcode == OPCODE_ADD || (*it)->opcode == OPCODE_BRANCH) {
            fu = ALU;
        }
        else if((*it)->opcode == OPCODE_MUL) {
            fu = MUL;
        }
        else {
            fu = MEM;
        }

        /* wake up and fire if reservation station if ready */
        if ((*it)->ready1 && (*it)->ready2) {
            /* enfore total store ordering before checking everything */
            if (fu == MEM) {
                uint64_t this_index = ((*it)->ls_addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);
                list<reserv_stat*>::iterator tso;
                for (tso = schedq.begin(); tso != it; tso ++) {
                    uint64_t tso_index = ((*tso)->ls_addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);
                    if (tso_index == this_index) {
                        /* index conflict with instruction earlier in queue - skip rest of iteration */
                        continue;
                    }
                }
            }

            for (uint64_t i = 0; i < NUM_FUS; i ++) {
                /* find open functional unit */
                if (scoreb[i]->type == fu && !scoreb[i]->busy) {
                    /* fire instruction */
                    scoreb[i]->busy = true;
                    scoreb[i]->dregs[0] = (*it)->dest;
                    scoreb[i]->dtags[0] = (*it)->tagd;
                    scoreb[i]->ls_addr = (*it)->ls_addr;
                    scoreb[i]->opcode = (*it)->opcode;

                    num_fired ++;
                }
            }
        }
    }

    if (num_fired == 0) {
        /* increment number of cycles where no instructions fired */
        stats->no_fire_cycles ++;
    }
}

/* moves instructions through pipelined FUs; removes instruction from sched queue when it has completed */
static void stage_exec(procsim_stats_t *stats) {
    /* iterate through the function units - first is alu */
    for (uint64_t i = 0; i < NUM_ALU; i ++) {
        /* send to result bus if it is not busy */
        if (!rbuses[i]->busy) {
            rbuses[i]->busy = true;
            rbuses[i]->tag = scoreb[i]->dtags[0];
            rbuses[i]->reg = scoreb[i]->dregs[0];

            /* set fu to not busy */
            scoreb[i]->busy = false;
        }
    }
    
    /* mul function units */
    for (uint64_t i = NUM_ALU; i < (NUM_ALU + NUM_MUL); i ++) {
        /* send to result bus if it is not busy */
        if (!rbuses[i]->busy) {
            rbuses[i]->busy = true;
            rbuses[i]->tag = scoreb[i]->dtags[2];
            rbuses[i]->reg = scoreb[i]->dregs[2];

            /* set fu to not busy */
            scoreb[i]->busy = false;

            /* move instructions along pipeline */
            scoreb[i]->dregs[2] = scoreb[i]->dregs[1];
            scoreb[i]->dregs[1] = scoreb[i]->dregs[0];
            scoreb[i]->dtags[2] = scoreb[i]->dtags[1];
            scoreb[i]->dtags[1] = scoreb[i]->dtags[0];
        }
    }

    /* mem function units */
    for (uint64_t i = (NUM_ALU + NUM_MUL); i < NUM_FUS; i ++) {
        if (scoreb[i]->opcode == OPCODE_LOAD) {
            uint64_t addr = scoreb[i]->ls_addr;
            uint64_t tag = (addr >> (INDEX_SIZE + DCACHE_B)) & ((1 << TAG_SIZE) - 1);
            uint64_t index = (addr >> DCACHE_B) & ((1 << INDEX_SIZE) - 1);

            if (scoreb[i]->stalls > 0) {
                if (scoreb[i]->stalls == L2_LATENCY_CYCLES) {
                    /* bring L2 data to cache */
                    dcache[index]->tag = tag;
                    dcache[index]->valid = true;

                    /* update stats */
                    stats->dcache_reads ++;

                    /* send to result bus if it is not busy */
                    if (!rbuses[i]->busy) {
                        rbuses[i]->busy = true;
                        rbuses[i]->tag = scoreb[i]->dtags[0];
                        rbuses[i]->reg = scoreb[i]->dregs[0];

                        /* set fu to not busy and reset stalls */
                        scoreb[i]->stalls = 0;
                        scoreb[i]->busy = false;
                    }
                }
                else if (scoreb[i]->stalls < L2_LATENCY_CYCLES) {
                    /* still waiting on L2 */
                    scoreb[i]->stalls ++;
                }
                else { /* already brought data in but result bus was busy */
                    /* send to result bus if it is not busy */
                    if (!rbuses[i]->busy) {
                        rbuses[i]->busy = true;
                        rbuses[i]->tag = scoreb[i]->dtags[0];
                        rbuses[i]->reg = scoreb[i]->dregs[0];

                        /* set fu to not busy and reset stalls */
                        scoreb[i]->stalls = 0;
                        scoreb[i]->busy = false;
                    }
                }
            }
            else {
                if (tag != dcache[index]->tag || !dcache[index]->valid) {
                    /* cache miss */
                    scoreb[i]->stalls ++;

                    /* update stats */
                    stats->dcache_read_misses ++;
                }
                else {
                    /* update stats */
                    stats->dcache_reads ++;
                    
                    /* cache hit - send to result bus if it is not busy */
                    if (!rbuses[i]->busy) {
                        rbuses[i]->busy = true;
                        rbuses[i]->tag = scoreb[i]->dtags[0];
                        rbuses[i]->reg = scoreb[i]->dregs[0];

                        /* set fu to not busy */
                        scoreb[i]->busy = false;
                    }
                }
            }
        }
    }

    for (uint64_t i = 0; i < NUM_FUS; i ++) {
        /* update messy register with result bus */
        if (rbuses[i]->busy && messy[rbuses[i]->reg]->tag == rbuses[i]->tag) {
            if (rbuses[i]->reg != -1) {
                messy[rbuses[i]->reg]->ready = true;
            }
            rbuses[i]->busy = false;
        }

        /* update rob with result buses */
        list<rob_entry*>::iterator it;
        for (it = rob.begin(); it != rob.end(); it ++) {
            if ((*it)->tag == rbuses[i]->tag) {
                (*it)->ready = true;

                /* delete completed instructions from scheduling queue */
                list<reserv_stat*>::iterator it;
                for (it = schedq.begin(); it != schedq.end(); it ++) {
                    if ((*it)->tagd == rbuses[i]->tag) {
                        schedq.erase(it);
                    }
                }
            }
        }
    }
}

/* pops instructions from head of ROB and returns number of instructions retired */
static uint64_t stage_state_update(procsim_stats_t *stats, bool *retired_interrupt_out) {
    uint64_t num_retired = 0;

    /* iterate through rob for length of retire width */
    for (uint64_t i = 0; i < MAX_RETIRE; i ++) {
        if (rob.size() > 0) {
            rob_entry * front = rob.front();
            if (front->ready) {
                num_retired ++;
                stats->instructions_retired ++;

                /* interrupt found, set retired interrupt flag to true and return */
                if (front->excpt) {
                    *retired_interrupt_out = true;
                    return num_retired;
                }

                /* architectural file is written to by non-store instructions */
                if (front->reg != -1) {
                    stats->arf_writes ++;
                }

                /* pop from rob */
                rob.pop_front();
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

    /* flush and reset all function units and result buses */
    for (uint64_t i = 0; i < NUM_FUS; i ++) {
        /* dcache is left as is - but pending loads not fulfilled */
        scoreb[i]->flush();
    }

    /* set all messy regs to ready */
    for (uint64_t i = 0; i < NUM_REGS; i ++) {
        messy[i]->flush(curr_tag++);
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

    /* initialize scoreboard to all fus not busy */
    scoreb = new funct_unit*[NUM_FUS];
    for (uint64_t i = 0; i < NUM_ALU; i ++) {
        scoreb[i] = new funct_unit(ALU);
    }
    for (uint64_t i = NUM_ALU; i < (NUM_ALU + NUM_MUL); i ++) {
        scoreb[i] = new funct_unit(MUL);
    }
    for (uint64_t i = (NUM_ALU + NUM_MUL); i < NUM_FUS; i ++) {
        scoreb[i] = new funct_unit(MEM);
    }

    /* set all dcache entries to empty and invalid */
    dcache = new cache_entry*[CACHE_SETS];
    for (uint64_t i = 0; i < CACHE_SETS; i ++) {
        dcache[i] = new cache_entry();
    }

    /* set all registers in messy to ready and empty */
    messy = new reg_entry*[NUM_REGS];
    for (uint64_t i = 0; i < NUM_REGS; i ++) {
        messy[i] = new reg_entry();
    }
}
 
/* calls the stage functions above in reverse order */
uint64_t procsim_do_cycle(procsim_stats_t *stats, bool *retired_interrupt_out) {

    /* retired_interrupt_out is set by stage_state_update() */
    uint64_t retired_this_cycle = stage_state_update(stats, retired_interrupt_out);

    if (*retired_interrupt_out) {
        /* after retiring an interrupt, flush pipeline immediately and pick up where sim left off in the next cycle */
        stats->interrupts++;
        flush_pipeline();
    } else {
        /* if interrupt was not retired, continue simulating the other pipeline stages */
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

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
    stats->rob_avg_usage += rob.size();

    /* return the number of instructions retired this cycle (including the interrupt if there was one) */
    return retired_this_cycle;
}

/* free allocated memory and calculate some final statistics */
void procsim_finish(procsim_stats_t *stats) {
    /* free allocated memory */
    for (uint64_t i = 0; i < NUM_FUS; i ++) {
        delete [] scoreb[i];
    }
    for (uint64_t i = 0; i < CACHE_SETS; i ++) {
        delete [] dcache[i];
    }
    for (uint64_t i = 0; i < NUM_REGS; i ++) {
        delete [] messy[i];
    }
    delete [] scoreb;
    delete [] dcache;
    delete [] messy;

    /* calculate final average use statistics */
    stats->dispq_avg_usage = stats->dispq_avg_usage / stats->cycles;
    stats->schedq_avg_usage = stats->schedq_avg_usage / stats->cycles;
    stats->rob_avg_usage = stats->rob_avg_usage / stats->cycles;

    /* calculate final cache statistics */
    stats->dcache_read_miss_ratio = stats->dcache_read_misses / stats->dcache_reads;
    stats->dcache_read_aat = 1 + stats->dcache_read_miss_ratio * L2_LATENCY_CYCLES;

    /* calculate final ipc */
    stats->ipc = stats->instructions_retired / stats->cycles;
}
