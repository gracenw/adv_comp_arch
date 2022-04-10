#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "hash_table.hpp"
#include "processor.hpp"
#include "settings.hpp"
#include "sim.hpp"

using namespace std;

Processor::Processor (ModuleID moduleID, Hash_table *cache, char *trace_file)
    : Module (moduleID, "Processor_")
{
    this->moduleID = moduleID;
    this->infile = fopen (trace_file, "r");
    this->my_cache = cache;
    this->end_of_trace = false;
    this->inbound_request = NULL;
    this->inbound_request_buf = NULL;
}

Processor::~Processor ()
{
    fclose (this->infile);
}

/** Done once at end of trace and no outstanding requests.  */
bool Processor::done ()
{
    return (end_of_trace && !outstanding_request);
}

void Processor::tick ()
{
    char c;
    paddr_t addr;

    if (inbound_request)
    {
        #ifdef DEBUG
    	printf("* COMPLETE -- PR: %d -- Clock: %" PRIu64 "\n",moduleID.nodeID, Global_Clock);
        #endif
    	assert (inbound_request->msg == DATA);
    	outstanding_request = false;
        delete inbound_request;
    }
    inbound_request = NULL;

    if (end_of_trace || outstanding_request)
        return;

    if (fscanf (infile, "%c 0x%llx\n", &c, (unsigned long long int*)&addr) == 2)
    {
        Mreq *request;

        #ifdef DEBUG
        printf("* FETCH -- PR: %d -- Clock: %" PRIu64 " -- %c 0x%" PRIx64 "\n", moduleID.nodeID, Global_Clock, c, addr);
        #endif

        switch (c) {
        case 'r': request = new Mreq (LOAD, addr, moduleID); break;
        case 'w': request = new Mreq (STORE, addr, moduleID); break;
        default:
            fatal_error ("Processor %d: unknown operation - %c", moduleID.nodeID, c);
        }
        
        my_cache->proc_request =  request;
        outstanding_request = true;
    }
    else
    {
        end_of_trace = true;
    }
}

void Processor::tock ()
{
	if (inbound_request_buf)
	{
		inbound_request = inbound_request_buf;
		inbound_request_buf = NULL;
	}
}

