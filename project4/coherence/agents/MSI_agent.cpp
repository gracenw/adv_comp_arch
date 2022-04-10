#include "MSI_agent.hpp"
#include "mreq.hpp"
#include "sim.hpp"
#include "hash_table.hpp"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MSI_agent::MSI_agent (Hash_table *my_table, Hash_entry *my_entry)
	: Agent (my_table, my_entry)
{
	this->state = MSI_CACHE_I;

}

MSI_agent::~MSI_agent ()
{
}

void MSI_agent::dump (void)
{
	const char *block_states[8] = {"X","I","S","M","IS","SM","IM"};
	printf ("MSI_agent - state: %s\n", block_states[state]);
}

void MSI_agent::process_cache_request (Mreq *request)
{
	switch (state) {
	case MSI_CACHE_I: do_cache_I(request); break;
	case MSI_CACHE_M: do_cache_M(request); break;
	case MSI_CACHE_S: do_cache_S(request); break;
	case MSI_CACHE_IS:
	case MSI_CACHE_IM:
	case MSI_CACHE_SM:
		do_cache_Trans(request); 
		break;

	default:
		fatal_error ("Invalid Cache State for MSI Protocol\n");
	}
}

void MSI_agent::process_fetch_request (Mreq *request)
{
	switch (state) {
	case MSI_CACHE_I:  do_fetch_I(request); break;
	case MSI_CACHE_M:  do_fetch_M(request); break;
	case MSI_CACHE_S: do_fetch_S(request); break;
	case MSI_CACHE_IS: do_fetch_IS(request); break;
	case MSI_CACHE_IM: do_fetch_IM(request); break;
	case MSI_CACHE_SM: do_fetch_SM(request); break;

	default:
		fatal_error ("Invalid Cache State for MSI Protocol\n");
	}
}

inline void MSI_agent::do_cache_I (Mreq *request)
{
	switch (request->msg) {
		// If we get a request from the processor we need to get the data
		case LOAD:
		/* Line up the GETS in the Bus' queue */
		send_GETS(request->addr);
			/* The IS state means that we have sent the GET message and we are now waiting
			 * on DATA */
		state = MSI_CACHE_IS;
		/* This is a cache miss */
		Sim->cache_misses++;
		break;
	case STORE:
		/* Line up the GETM in the Bus' queue */
		send_GETM(request->addr);
		/* The IM state means that we have sent the GET message and we are now waiting
		 * on DATA */
		state = MSI_CACHE_IM;
		/* This is a cache miss */
		Sim->cache_misses++;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: I state shouldn't see this message\n");
	}



}

inline void MSI_agent::do_cache_S (Mreq *request)
{
	switch (request->msg) {
		/* The S state means we have the data.  Therefore any request
		 * from the processor (read) can be immediately satisfied. */
	case LOAD:
		// This is how you send data back to the processor to finish the request
		// 		// Note: There was no need to send anything on the bus on a hit.
		send_DATA_to_proc(request->addr);
		break;
	case STORE:
		/* Line up the GETM in the Bus' queue */
		send_GETM(request->addr);
		/* The SM state means that we have sent the GET message and we are now waiting
		 * on DATA */
		state = MSI_CACHE_SM;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: S state shouldn't see this message\n");
	}

}


inline void MSI_agent::do_cache_M (Mreq *request)
{
	switch (request->msg) {
		/* The M state means we have the data and we can modify it.  Therefore any request
		 * from the processor (read or write) can be immediately satisfied. */
	case LOAD:
	case STORE:
		// This is how you send data back to the processor to finish the request
		// Note: There was no need to send anything on the bus on a hit.
		send_DATA_to_proc(request->addr);
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: M state shouldn't see this message\n");
	}

}

inline void MSI_agent::do_cache_Trans (Mreq *request)
{
	switch (request->msg) {
		
	case LOAD:
	case STORE:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Should only have one outstanding request per processor!");
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client shouldn't see this message\n");
	}
}

inline void MSI_agent::do_fetch_I (Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM:
	case DATA:
	    
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: I state shouldn't see this message\n");
	}


}

inline void MSI_agent::do_fetch_S (Mreq *request)
{
	switch (request->msg) {
	case GETS:	
	case GETM:
	case DATA:
		fatal_error("Client: fetch_S state shouldn't see this message\n");
		break;
	case MREQ_INVALID:
		send_INVACK(request->addr);
		state = MSI_CACHE_I;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: M state shouldn't see this message\n");
	}
}


inline void MSI_agent::do_fetch_M (Mreq *request)
{
	switch (request->msg) {
	case GETS:
		
		send_DATA_on_ptpNetwork(request->addr, request->src_mid);
		Sim->cache_to_cache_transfers++;
		state = MSI_CACHE_S;
		break;
	case GETM:
	case MREQ_INVALID:
		send_DATA_on_ptpNetwork(request->addr, request->src_mid);
		Sim->cache_to_cache_transfers++;
		state = MSI_CACHE_I;
		break;
	case DATA:
		fatal_error("Should not see data for this line!  I have the line!");
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: M state shouldn't see this message\n");
	}       
}

inline void MSI_agent::do_fetch_IM(Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM:
		fatal_error("Client: fetch_IM state shouldn't see this message\n");
		break;
	case DATA:
		
		send_DATA_to_proc(request->addr);
		state = MSI_CACHE_M;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: I state shouldn't see this message\n");
	}
}

inline void MSI_agent::do_fetch_IS(Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM:
		
		fatal_error("Client: fetch_IS state shouldn't see this message\n");
		break;
	case DATA:
		
		send_DATA_to_proc(request->addr);
		state = MSI_CACHE_S;
		break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: IS state shouldn't see this message\n");
	}
}

inline void MSI_agent::do_fetch_SM(Mreq *request)
{
	switch (request->msg) {
	case GETS:
	case GETM:
		fatal_error("Client: fetch_SM state shouldn't see this message\n");
		break;
	case MREQ_INVALID:
			send_INVACK(request->addr);
			state = MSI_CACHE_IM;
			break;
	case UPGRADE_M:
			send_DATA_to_proc(request->addr);
			state = MSI_CACHE_M;
			break;
	default:
		request->print_msg(my_table->moduleID, "ERROR");
		fatal_error("Client: SM state shouldn't see this message\n");
	}
}
