#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <inttypes.h>

#include "hash_table.hpp"
#include "processor.hpp"
#include "dir_controller.hpp"
#include "module.hpp"
#include "mreq.hpp"
#include "settings.hpp"
#include "sim.hpp"
#include "types.hpp"

extern Sim_settings settings;

/** Fatal Error.  */
void fatal_error (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
    
    /** Enable debugging by asserting zero.  */
    assert (0 && "Fatal Error");
    exit (-1);
}

Simulator::Simulator ()
{
    /** Seed random number generator.  */
    srandom (1023);
    
    /** Set global_clock to cycle zero.  */
    global_clock = 0;

    /** Allocate ptpNetwork.  */
    ptpnetwork = new ptpNetwork (settings.num_nodes);
    assert (ptpnetwork && "Sim error: Unable to alloc ptpnetwork.");

    Nd = new Node*[settings.num_nodes+1];

    /** Allocate processors.  */
    for (int node = 0; node < settings.num_nodes; node++)
    {
        char trace_file[100];
        sprintf (trace_file, "%s/p%d.trace", settings.trace_dir, node);

        Nd[node] = new Node (node);
        Nd[node]->build_processor (trace_file);
    }

    /** Allocate directory controllers.  */
    Nd[settings.num_nodes] = new Node (settings.num_nodes);
    Nd[settings.num_nodes]->build_directory_controller ();

    /*Set the memory controllers ModuleID */
     for (int node = 0; node < settings.num_nodes; node++)
    { 
       Nd[node]->mod[L1_M]->DC_moduleID = Nd[settings.num_nodes]->mod[DC_M]->moduleID;
    }


    cache_misses = 0;
    silent_upgrades = 0;
    cache_to_cache_transfers = 0;
    cache_accesses = 0;
}

Simulator::~Simulator ()
{
    for (int i = 0; i <= settings.num_nodes; i++)
        delete Nd[i];

    delete [] Nd;    
}

void Simulator::dump_stats ()
{
    #ifdef DEBUG
    for (int i=0; i < settings.num_nodes; i++)
    {
    	get_L1(i)->dump_hash_table();
    }
    #endif
    printf("\nRun Time:         %8" PRIu64 " cycles\n",global_clock);
    printf("Cache Misses:     %8ld misses\n",cache_misses);
    printf("Cache Accesses:   %8ld accesses\n",cache_accesses);
    printf("Silent Upgrades:  %8ld upgrades\n",silent_upgrades);
    printf("$-to-$ Transfers: %8ld transfers\n",cache_to_cache_transfers);
}

void Simulator::run ()
{
    bool done;

    /** This must match what's in enums.h.  */
    const char *cp_str[9] = {"CACHE_PRO","MI_PRO","MSI_PRO","MESI_PRO",
							 "MOESI_PRO","MOSI_PRO","NULL_PRO","MEM_PRO"};

    printf("CS6290 Sim - Begins  ");
    printf(" Cores: %d", settings.num_nodes);
    printf(" Protocol: %s\n", cp_str[settings.protocol]);

    /** Main run loop.  */
    static const timestamp_t max_cycles = 1500000;
    done = false;
    while (!done)
    {
        ptpnetwork->tick ();

        for (int i = 0; i <= settings.num_nodes; i++)
            Nd[i]->tick_cache ();

        for (int i = 0; i <= settings.num_nodes; i++)
            Nd[i]->tick_pr ();

        for (int i = 0; i <= settings.num_nodes; i++)
            Nd[i]->tick_dc ();
        
        for (int i = 0; i <= settings.num_nodes; i++)
			Nd[i]->tock_pr ();

        global_clock++;

        done = true;
        for (int i = 0; i < settings.num_nodes; i++)
            if (!get_PR(i)->done ())
            {
                done = false;
                break;        
            }

        if (!done && global_clock >= max_cycles) {
            fatal_error("Simulator has been running for %" PRIu64 " cycles and has not yet "
                        "finished. It should never need this many cycles --- is there a deadlock?\n",
                        global_clock);
        }
    }

    #ifdef DEBUG
    printf("\nSimulation Finished\n");
    #endif
    dump_stats();
}

Processor* Simulator::get_PR (int node)
{
    return (Processor *)(Nd[node]->mod[PR_M]);
}
Hash_table* Simulator::get_L1 (int node)
{
    return (Hash_table *)(Nd[node]->mod[L1_M]);
}
Directory_controller* Simulator::get_DC (int node)
{
    return (Directory_controller *)(Nd[node]->mod[DC_M]);
}

/** Debug.  */
void Simulator::dump_outstanding_requests (int nodeID)
{
    assert (nodeID < settings.num_nodes);   
}

void Simulator::dump_cache_block (int nodeID, paddr_t addr)
{
    assert (get_L1(nodeID));
    get_L1 (nodeID)->dump_hash_entry (addr);
}
