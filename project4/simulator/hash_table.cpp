#include <assert.h>
#include <iostream>
#include <math.h>
#include <string.h>

#include "hash_table.hpp"
#include "MI_agent.hpp"
#include "MSI_agent.hpp"
#include "MESI_agent.hpp"
#include "settings.hpp"
#include "sharers.hpp"
#include "sim.hpp"
#include "types.hpp"
#include "processor.hpp"

using namespace std;

/***************************************************************************
 * Hash_entry constructor, destructor, and functions.
 ***************************************************************************/
Hash_entry::Hash_entry (Hash_table *t, paddr_t tag)
{
    this->my_table = t;
    this->tag = tag;

    switch (my_table->protocol) {
    case MI_PRO:
        agent = new MI_agent (my_table, this);
        break;
    case MSI_PRO:
        agent = new MSI_agent (my_table, this);
        break;
    case MESI_PRO:
    	agent = new MESI_agent (my_table, this);
    	break;
    default:
        fatal_error ("Hash_entry: Unknown coherence protocol!\n");
    }
}

Hash_entry::~Hash_entry (void)
{
}

void Hash_entry::process_request_fetch (Mreq *request)
{
    assert (agent);
    assert (request);

    agent->process_fetch_request (request);
}

void Hash_entry::process_request_processor (Mreq *request)
{
    agent->process_cache_request (request);
}

void Hash_entry::dump (void)
{
    printf ("Addr: 0x%llx ", (unsigned long long)tag);
    agent->dump ();
}

/***************************************************************************
 * Hash constructor, destructor, and fucntions.
 ***************************************************************************/
Hash_table::Hash_table (ModuleID moduleID, const char *name,
                        int size, int assoc, int blocksize, int mshrs,
                        int hit_time, protocol_t protocol)
	: Module (moduleID, name)
{
    /** Sanity check.  */
    /** Note, we do allow the hash size and assoc to be non-powers of 2. */
    if (!size)
        fatal_error ("%s: Invalid hash size - %d\n", name, size);

    if (!assoc)
        fatal_error ("%s: Invalid hash assoc - %d\n", name, assoc);

    if (!blocksize || !ISPOW2 (blocksize))
        fatal_error ("%s: Invalid hash blocksize", name);

    /** Parameters.  */
    this->size = size;
    this->assoc = assoc;
    this->sets = (size) / (assoc * blocksize);
    this->blocksize = blocksize;
    this->mshrs = mshrs;
    this->hit_time = hit_time;
    this->protocol = protocol;


    /** Calculate tag and index masks once.  */
    num_index_bits = (int) log2 (sets);
    num_offset_bits = (int) log2 (blocksize);

    assert ((1 << num_index_bits) == sets && "sets not power of 2?");
    assert ((1 << num_offset_bits) == blocksize && "blocksize not power of 2?");

    tag_mask = ~0x0;
    tag_mask = tag_mask << (num_index_bits + num_offset_bits);

    index_mask = ~0x0;
    index_mask = index_mask << (num_offset_bits);
    index_mask = index_mask & ~tag_mask;

    my_entries.clear ();
}

/** Destructor.  */
Hash_table::~Hash_table (void)
{

}

/*****************************
 * Hash tick/tock functions.
 *****************************/
void Hash_table::tick (void)
{
    Mreq *request;
    Hash_entry *entry;

    /** Request from processor.  */
    if (proc_request)
    {
        #ifdef DEBUG
    	printf("** PROC REQUEST -- ");
    	proc_request->print_msg (moduleID, NULL);
        #endif
    	Sim->cache_accesses++;
        entry = get_entry (proc_request->addr);
        assert (entry);
        entry->process_request_processor (proc_request);
        delete proc_request;
        proc_request = NULL;
    }

    /** Request from ptpNetwork.  */
    request = fetch_input_port ();
    if (request)
    {
    	//if (request->msg == DATA && request->dest_mid != this->moduleID)
    	//{
    	//	return;
    	//}

        #ifdef DEBUG
    	printf("*** RECEIVE REQUEST -- ");
        request->print_msg (moduleID, NULL);
        #endif
        entry = get_entry (request->addr);
        assert (entry);
        entry->process_request_fetch (request);
    }
}

/** Request sent from processor.  */
void Hash_table::processor_request (Mreq *request)
{
    assert (proc_request == NULL);
    proc_request = request;
}

void Hash_table::tock (void)
{
    fatal_error ("%s - tock should never be called!", name);
}

/*******************************
 * Generic Hash_table functions.
 *******************************/
Hash_entry* Hash_table::get_entry (paddr_t addr)
{
    MAP<paddr_t, Hash_entry*>::iterator it;
    
    it = my_entries.find (addr);
    if (it == my_entries.end ())
    {
        my_entries.insert(pair<paddr_t, Hash_entry*>(addr,new Hash_entry (this, addr)));       
    }
    return my_entries[addr];
}

bool Hash_table::write_to_proc (Mreq *mreq)
{
	Processor * pr = (Processor*)Sim->get_PR(moduleID.nodeID);
	mreq->src_mid = moduleID;

	assert (!pr->inbound_request_buf);

	pr->inbound_request_buf = mreq;

	return true;
}

bool Hash_table::write_to_ptpNetwork (Mreq *mreq)
{
	mreq->src_mid = moduleID;
        mreq->dest_mid = DC_moduleID;
	return this->write_output_port(mreq);
}

/********
 * Debug.
 ********/
void Hash_table::dump_hash_entry (paddr_t addr)
{
    Hash_entry *entry;

    entry = get_entry (addr);
    if (entry)
        entry->dump ();
}

void Hash_table::dump_hash_table ()
{
	MAP<paddr_t, Hash_entry*>::iterator it;

	printf("Cache %d Contents:\n",moduleID.nodeID);

	for (it = my_entries.begin(); it != my_entries.end(); it++)
	{
		it->second->dump();
	}

}

void Hash_table::print_config (void)
{
    printf("%s CONFIGURATION\n", name);
    printf(" blocksize:         %d bytes\n", blocksize);
}

