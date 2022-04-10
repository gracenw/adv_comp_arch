#include "MOSIF_agent.hpp"
#include "mreq.hpp"
#include "sim.hpp"
#include "hash_table.hpp"

extern Simulator *Sim;

/*************************
 * Constructor/Destructor.
 *************************/
MOSIF_agent::MOSIF_agent (Hash_table *my_table, Hash_entry *my_entry)
    : Agent (my_table, my_entry)
{
}

MOSIF_agent::~MOSIF_agent ()
{    
}

void MOSIF_agent::dump (void)
{
    const char *block_states[9] = {"X","I","S","E","O","M","F"};
    printf ("MOSIF_agent - state: %s\n", block_states[state]);
}

void MOSIF_agent::process_cache_request (Mreq *request)
{
	switch (state) {

    default:
        fatal_error ("Invalid Cache State for MOSIF Protocol\n");
    }
}

void MOSIF_agent::process_fetch_request (Mreq *request)
{
	switch (state) {

    default:
    	fatal_error ("Invalid Cache State for MOSIF Protocol\n");
    }
}

inline void MOSIF_agent::do_cache_F (Mreq *request)
{

}

inline void MOSIF_agent::do_cache_I (Mreq *request)
{

}

inline void MOSIF_agent::do_cache_S (Mreq *request)
{

}

inline void MOSIF_agent::do_cache_O (Mreq *request)
{

}

inline void MOSIF_agent::do_cache_M (Mreq *request)
{

}

inline void MOSIF_agent::do_fetch_F (Mreq *request)
{

}

inline void MOSIF_agent::do_fetch_I (Mreq *request)
{

}

inline void MOSIF_agent::do_fetch_S (Mreq *request)
{

}

inline void MOSIF_agent::do_fetch_O (Mreq *request)
{

}

inline void MOSIF_agent::do_fetch_M (Mreq *request)
{

}
