#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "mreq.hpp"
#include "settings.hpp"
#include "sim.hpp"

using namespace std;

/***************
 * Constructor.
 ***************/
Mreq::Mreq (message_t msg, paddr_t addr, ModuleID src_mid, ModuleID dest_mid)
{
    this->msg = msg;
    this->addr = addr & ((~0x0) << settings.cache_line_size_log2);
    this->src_mid = src_mid;
    this->dest_mid = dest_mid;
    this->fwd_mid = {-1, INVALID_M};
    this->INV_ACK_count = 0;
    this->req_time = Global_Clock;
    this->stalled = false;
    this->preq =NULL;
}

Mreq::~Mreq(void)
{
}

void Mreq::print_msg (ModuleID mid, const char *add_msg)
{
    print_id ("node", mid);
    print_id ("src", src_mid);
    print_id ("dest", dest_mid);
    printf("tag: 0x%8" PRIx64 " clock: %8" PRIu64 " ", addr>>settings.cache_line_size_log2, Global_Clock);
    printf(" %8s\n", Mreq::message_t_str[msg]);
}

void Mreq::dump ()
{
    printf("Request Dump ");
    print_id ("src", src_mid);
    print_id ("dest", dest_mid);
    printf("0x%8" PRIx64 " Clock: %8" PRIu64 " %20s\n",
             addr, Global_Clock, Mreq::message_t_str[msg]);
}
