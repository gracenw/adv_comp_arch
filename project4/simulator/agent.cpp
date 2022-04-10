#include <inttypes.h>
#include "agent.hpp"
#include "sharers.hpp"
#include "hash_table.hpp"
#include "sim.hpp"

Agent::Agent (Hash_table *my_table, Hash_entry *my_entry)
{
    this->my_table = my_table;
    this->my_entry = my_entry;
}

Agent::~Agent ()
{
}

void Agent::send_GETM(paddr_t addr)
{
	/* Create a new message to send on the ptpNetwork */
	Mreq * new_request;
	/* The arguments to Mreq are -- msg, address, src_id (optional), dest_id (optional) */
	new_request = new Mreq(GETM,addr);
	/* This will but the message in the ptpNetwork' arbitration queue to sent */
	this->my_table->write_to_ptpNetwork(new_request);
}

void Agent::send_GETS(paddr_t addr)
{
	/* Create a new message to send on the ptpNetwork */
	Mreq * new_request;
	/* The arguments to Mreq are -- msg, address, src_id (optional), dest_id (optional) */
	new_request = new Mreq(GETS,addr);
	/* This will but the message in the ptpNetwork' arbitration queue to sent */
	this->my_table->write_to_ptpNetwork(new_request);
}

void Agent::send_DATA_on_ptpNetwork(paddr_t addr, ModuleID dest)
{
	/* Create a new message to send on the ptpNetwork */
	Mreq * new_request;
	/* The arguments to Mreq are -- msg, address, src_id (optional), dest_id (optional) */
	// When DATA is sent on the ptpNetwork it _MUST_ have a destination module
	new_request = new Mreq(DATA, addr, my_table->moduleID, dest);
    #ifdef DEBUG
	/* Debug Message -- DO NOT REMOVE or you won't match the validation runs */
	printf("**** DATA_SEND Cache: %d -- Clock: %" PRIu64 "\n",my_table->moduleID.nodeID,Global_Clock);
    #endif
	/* This will but the message in the ptpNetwork' arbitration queue to sent */
	this->my_table->write_to_ptpNetwork(new_request);

//	Sim->cache_to_cache_transfers++;
}

void Agent::send_DATA_to_proc(paddr_t addr)
{
	/* Create a new message to send on the ptpNetwork */
	Mreq * new_request;
	/* The arguments to Mreq are -- msg, address, src_id (optional), dest_id (optional) */
	// When data is sent from a cache to proc, there is no need to set the src and dest
	new_request = new Mreq(DATA,addr);
	/* This writes the message into the processor's input buffer.  The processor
	 * only expects to ever receive DATA messages
	 */
	this->my_table->write_to_proc(new_request);
}

// send an invalidation ack from a processor to the directory
void Agent::send_INVACK(paddr_t addr)
{
	/* Create a new message to send on the ptpNetwork */
	Mreq * new_request;
	/* The arguments to Mreq are -- msg, address, src_id (optional), dest_id (optional) */
	new_request = new Mreq(INVACK,addr);
	/* This will but the message in the ptpNetwork' arbitration queue to sent */
	this->my_table->write_to_ptpNetwork(new_request);
}

// send a silent upgrade notification from a processor to the directory
void Agent::send_SILENTUPGRADE(paddr_t addr)
{
	/* Create a new message to send on the ptpNetwork */
	Mreq * new_request;
	/* The arguments to Mreq are -- msg, address, src_id (optional), dest_id (optional) */
	new_request = new Mreq(SILENT_UPGRADE_EM,addr);
	/* This will but the message in the ptpNetwork' arbitration queue to sent */
	this->my_table->write_to_ptpNetwork(new_request);
}

void Agent::set_shared_line ()
{
	
}

bool Agent::get_shared_line ()
{
	// making the compiler happy
	return false;
}
