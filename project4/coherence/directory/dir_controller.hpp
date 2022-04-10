#ifndef MEM_MAIN_H_
#define MEM_MAIN_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "module.hpp"
#include "mreq.hpp"
#include "settings.hpp"
#include "types.hpp"
#include "agent.hpp"

using namespace std;
class Dir_table;

enum DIR_state { 
    M,  // Modified
    O,  // Owner
    S,  // Shared
    I,  // Invalid
    F,  // Forwarder
    FS, // Transient state from F->S
    FM, // Transient state from F->M
    SM, // Transient state from S->M
    MO, // Transient state from M->O
    SS, // Transient state from S->S
    MS, // Transient state from M->S
    MM, // Transient state from M->M
    OM, // Transient state from O->M
    E,  // Exclusive
    ES, // Transient state from E->S
    EM, // Transient state from E->M
    OO  // Transient state from O->O
    };

class Dir_entry {
public:
    Dir_entry(Dir_table *t, paddr_t tag);
    virtual ~Dir_entry (void);

    Dir_table *dir_table;
    paddr_t tag;

    bool Dirty_bit;
    bool Presence_bits[16];
    DIR_state entry_state; 
    ModuleID sharers[16];
    int share_num;
    ModuleID reqID_in_transient;

    ModuleID Owner;
    ModuleID Fowarder;
    int inv_sent;
    bool GETM_nodata;
    bool nodata_transfer;
};


class Dir_table {
public:

   Dir_table (ModuleID moduleID, const char *name);
   virtual ~Dir_table (void);
   MAP<paddr_t, Dir_entry*> dir_entries;
   Dir_entry* get_entry (paddr_t addr);
   protocol_t protocol;
};


class Directory_controller : public Module
{
public:
	Directory_controller(ModuleID moduleID, int miss_time);
	~Directory_controller();

    void MI_tick();
    void MSI_tick();
    void MESI_tick();
    void MOSIF_tick();

    void print_dir_queue();

    int miss_time;

    bool request_in_progress;
    bool send_DATA_E;
    bool send_DATA_F;

    timestamp_t data_time;
    paddr_t data_addr;
    ModuleID data_target;
    Dir_table* dir_table; 
	void tick();
	void tock();
};

#endif /* MEM_MAIN_H_ */
