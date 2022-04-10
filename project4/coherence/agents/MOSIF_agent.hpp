#ifndef _MOSIF_CACHE_H
#define _MOSIF_CACHE_H

#include "types.hpp"
#include "enums.hpp"
#include "module.hpp"
#include "mreq.hpp"
#include "agent.hpp"

/** Cache states.  */
typedef enum {
    MOSIF_CACHE_I = 1,
    MOSIF_CACHE_S,
    MOSIF_CACHE_O,
    MOSIF_CACHE_M,
    MOSIF_CACHE_F,
} MOSIF_cache_state_t;

class MOSIF_agent : public Agent {
public:
    MOSIF_agent (Hash_table *my_table, Hash_entry *my_entry);
    ~MOSIF_agent ();

    MOSIF_cache_state_t state;
    
    void process_cache_request (Mreq *request);
    void process_fetch_request (Mreq *request);
    void dump (void);

    inline void do_cache_F (Mreq *request);
    inline void do_cache_I (Mreq *request);
    inline void do_cache_S (Mreq *request);
    inline void do_cache_O (Mreq *request);
    inline void do_cache_M (Mreq *request);

    inline void do_fetch_F (Mreq *request);
    inline void do_fetch_I (Mreq *request);
    inline void do_fetch_S (Mreq *request);
    inline void do_fetch_O (Mreq *request);
    inline void do_fetch_M (Mreq *request);
};

#endif // _MOSIF_CACHE_H
