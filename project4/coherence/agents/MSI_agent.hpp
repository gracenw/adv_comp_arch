#ifndef _MSI_CACHE_H
#define _MSI_CACHE_H

#include "types.hpp"
#include "enums.hpp"
#include "module.hpp"
#include "mreq.hpp"
#include "agent.hpp"

/** Cache states.  */
typedef enum {
    MSI_CACHE_I = 1,
    MSI_CACHE_S,
    MSI_CACHE_M,
    MSI_CACHE_IS,
    MSI_CACHE_SM,
    MSI_CACHE_IM
} MSI_cache_state_t;

class MSI_agent : public Agent {
public:
    MSI_agent (Hash_table *my_table, Hash_entry *my_entry);
    ~MSI_agent ();

    MSI_cache_state_t state;

    void process_cache_request (Mreq *request);
    void process_fetch_request (Mreq *request);
    void dump (void);

    inline void do_cache_I (Mreq *request);
    inline void do_cache_S (Mreq *request);
    inline void do_cache_M (Mreq *request);
    inline void do_cache_Trans(Mreq *request);
    inline void do_fetch_I (Mreq *request);
    inline void do_fetch_S (Mreq *request);
    inline void do_fetch_M (Mreq *request);
    inline void do_fetch_IM(Mreq *request);
    inline void do_fetch_IS(Mreq *request);
    inline void do_fetch_SM(Mreq *request);
};

#endif // _MSI_CACHE_H
