#include "dir_controller.hpp"
#include "sim.hpp"


Dir_entry::Dir_entry (Dir_table *t, paddr_t tag)
{   
    this->dir_table = t;
    this->tag = tag;
 
    Dirty_bit = false;
    for(int i = 0;i>16;i++){
        Presence_bits[i] = false;
    }

    entry_state = I;
    share_num =0;
    inv_sent = 0;
    GETM_nodata = false;
    nodata_transfer = false;
}

Dir_table::Dir_table(ModuleID moduleID, const char *name)
{
 dir_entries.clear ();


}

Dir_entry::~Dir_entry (void)
{
}


Dir_entry* Dir_table::get_entry (paddr_t addr)
{   
    MAP<paddr_t, Dir_entry*>::iterator it;
    
    it = dir_entries.find (addr);
    if (it == dir_entries.end ())
    {
         #ifdef DEBUG
         printf("**** Dir entry not found -- Clock: %lu\n",Global_Clock);
         #endif
  
        dir_entries.insert(pair<paddr_t, Dir_entry*>(addr,new Dir_entry (this, addr)));
    }
    return dir_entries[addr];
}

Dir_table::~Dir_table (void)
{

}

Directory_controller::Directory_controller(ModuleID moduleID, int miss_time)
	: Module (moduleID, "DC_")
{
	this->miss_time = miss_time;
	request_in_progress = false;
        send_DATA_E = false;
	data_time = 0;
	data_target = {-1,INVALID_M}; // (ModuleID)
	dir_table = new Dir_table(moduleID, "DIR_");
}

Directory_controller::~Directory_controller()
{
}

void Directory_controller::tick()
{
    if (settings.protocol == MI_PRO) {
        MI_tick();
    } else if (settings.protocol == MSI_PRO) {
        MSI_tick();
    } else if (settings.protocol == MESI_PRO) {
        MESI_tick();
    } else if (settings.protocol == MOSIF_PRO) {
        MOSIF_tick();
    } else {
        fatal_error ("Wrong Protocol input\n");
    }
}

void Directory_controller::tock() {
    fatal_error("Memory controller tock should never be called!\n");
}
