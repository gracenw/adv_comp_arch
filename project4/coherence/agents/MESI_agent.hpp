#ifndef _MESI_CACHE_H
#define _MESI_CACHE_H

#include "types.hpp"
#include "enums.hpp"
#include "module.hpp"
#include "mreq.hpp"
#include "agent.hpp"

/** Cache states.  */
typedef enum {
    MESI_CACHE_I = 1,
    MESI_CACHE_S,
    MESI_CACHE_E,
    MESI_CACHE_M,
    MESI_CACHE_IM, 
    MESI_CACHE_IS, 
    MESI_CACHE_SM
} MESI_cache_state_t;

class MESI_agent : public Agent {
public:
    MESI_agent (Hash_table *my_table, Hash_entry *my_entry);
    ~MESI_agent ();

    MESI_cache_state_t state;
    
    void process_cache_request (Mreq *request);
    void process_fetch_request (Mreq *request);
    void dump (void);

    inline void do_cache_I (Mreq *request);
    inline void do_cache_S (Mreq *request);
    inline void do_cache_E (Mreq *request);
    inline void do_cache_M (Mreq *request);

    inline void do_fetch_I (Mreq *request);
    inline void do_fetch_S (Mreq *request);
    inline void do_fetch_E (Mreq *request);
    inline void do_fetch_M (Mreq *request);
    inline void do_fetch_IM (Mreq *request);
    inline void do_fetch_IS (Mreq *request);
    inline void do_fetch_SM (Mreq *request);
};

#endif // _MESI_CACHE_H
