#ifndef PTPNETWORK_H_
#define PTPNETWORK_H_

#include "types.hpp"

class Mreq;

class ptpNetwork{
public:
    ptpNetwork(int num_nd);
    ~ptpNetwork();

    LIST <Mreq *>pending_requests;
    LIST <Mreq *>node_requests[17]; 
    void tick ();
    bool  ptpNetwork_pushback(Mreq *request, int nodeID);
    bool  ptpNetwork_request (Mreq * request);
    Mreq* ptpNetwork_fetch(int nodeID);
    Mreq* ptpNetwork_read(int nodeID);
    void ptpNetwork_pop(int nodeID);
    int num_nodes; 
};

#endif
