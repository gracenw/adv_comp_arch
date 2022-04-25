#include "MESI_agent.hpp"
#include "mreq.hpp"
#include "sim.hpp"
#include "hash_table.hpp"

extern Simulator *Sim;

/* constructor & destructor */

MESI_agent::MESI_agent(Hash_table *my_table, Hash_entry *my_entry): Agent(my_table, my_entry) {
    this->state = MESI_CACHE_I;
}

MESI_agent::~MESI_agent() {}

/* class functions */

void MESI_agent::dump (void) {
    const char * block_states[5] = {"X","I","S","E","M"};
    printf("MESI_agent - state: %s\n", block_states[state]);
}

void MESI_agent::process_cache_request(Mreq *request) {
	switch (state) {
        case MESI_CACHE_M:  
            do_cache_M(request); 
            break;
        case MESI_CACHE_E:  
            do_cache_E(request); 
            break;
        case MESI_CACHE_S: 
            do_cache_S(request); 
            break;
        case MESI_CACHE_I:  
            do_cache_I(request); 
            break;
        case MESI_CACHE_IM:  
            break;
        case MESI_CACHE_IS:  
            break;
        case MESI_CACHE_SM:  
            break;
        default:
            fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

void MESI_agent::process_fetch_request(Mreq *request) {
	switch (state) {
        case MESI_CACHE_M:  
            do_fetch_M(request); 
            break;
        case MESI_CACHE_E:  
            do_fetch_E(request); 
            break;
        case MESI_CACHE_S: 
            do_fetch_S(request); 
            break;
        case MESI_CACHE_I:  
            do_fetch_I(request); 
            break;
        case MESI_CACHE_IM:  
            do_fetch_IM(request);
            break;
        case MESI_CACHE_IS:  
            do_fetch_IS(request);
            break;
        case MESI_CACHE_SM:  
            do_fetch_SM(request);
            break;
        default:
            fatal_error ("Invalid Cache State for MESI Protocol\n");
    }
}

inline void MESI_agent::do_cache_M (Mreq *request) {
    switch (request->msg) {
        case STORE:

            break;
        case LOAD:
    
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: M state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_cache_E (Mreq *request) {
    switch (request->msg) {
        case STORE:
        
            break;
        case LOAD:

            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: E state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_cache_S (Mreq *request) {
    switch (request->msg) {
        case STORE:
        
            break;
        case LOAD:
        
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_cache_I (Mreq *request) {
    switch (request->msg) {
        case STORE:
        
            break;
        case LOAD:
        
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_fetch_M (Mreq *request) {
    switch (request->msg) {
        case GETM:
        
            break;
        case GETS:
        
            break;
        case DATA:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: M state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_fetch_E (Mreq *request) {
    switch (request->msg) {
        case GETM:
        
            break;
        case GETS:
        
            break;
        case DATA:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: E state shouldn't see this message\n");
    }
}


inline void MESI_agent::do_fetch_S (Mreq *request) {
    switch (request->msg) {
        case GETM:
        
            break;
        case GETS:
        
            break;
        case DATA:
            break;
        case MREQ_INVALID:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: S state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_fetch_I (Mreq *request) {
    switch (request->msg) {
        case GETM:
            break;
        case GETS:
            break;
        case DATA:
            break;
        case DATA_E:
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: I state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_fetch_IM (Mreq *request) {
    switch (request->msg) {
        case GETM:
            break;
        case GETS:
            break;
        case DATA:
        
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: IM state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_fetch_IS (Mreq *request) {
    switch (request->msg) {
        case GETM:
            break;
        case GETS:
            break;
        case DATA:
        
            break;
        case DATA_E:
        
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: IS state shouldn't see this message\n");
    }
}

inline void MESI_agent::do_fetch_SM (Mreq *request) {
    switch (request->msg) {
        case GETM:
            break;
        case GETS:
        
            break;
        case DATA:
        
            break;
        default:
            request->print_msg (my_table->moduleID, "ERROR");
            fatal_error ("Client: SM state shouldn't see this message\n");
    }
}
