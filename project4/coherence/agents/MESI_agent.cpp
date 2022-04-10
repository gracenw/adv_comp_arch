#include "MESI_agent.hpp"
#include "mreq.hpp"
#include "sim.hpp"
#include "hash_table.hpp"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MESI_agent::MESI_agent (Hash_table *my_table, Hash_entry *my_entry)
    : Agent (my_table, my_entry)
{
}

MESI_agent::~MESI_agent ()
{    
}

void MESI_agent::dump (void)
{
    const char *block_states[5] = {"X","I","S","E","M"};
    printf("MESI_agent - state: %s\n", block_states[state]);
}

void MESI_agent::process_cache_request (Mreq *request)
{
	switch (state) {

    default:
        fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

void MESI_agent::process_fetch_request (Mreq *request)
{
	switch (state) {


    default:
    	fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

inline void MESI_agent::do_cache_I (Mreq *request)
{

}

inline void MESI_agent::do_cache_S (Mreq *request)
{

}

inline void MESI_agent::do_cache_E (Mreq *request)
{

}

inline void MESI_agent::do_cache_M (Mreq *request)
{

}

inline void MESI_agent::do_fetch_I (Mreq *request)
{

}

inline void MESI_agent::do_fetch_S (Mreq *request)
{

}

inline void MESI_agent::do_fetch_E (Mreq *request)
{

}

inline void MESI_agent::do_fetch_M (Mreq *request)
{

}
