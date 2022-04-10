#include "ptpnetwork.hpp"
#include "mreq.hpp"

ptpNetwork::ptpNetwork(int num_nd)
{
 num_nodes = num_nd; 
}

ptpNetwork::~ptpNetwork()
{
}

void ptpNetwork::tick()
{

	while (!pending_requests.empty())
	{
            Mreq *current_request;
	    current_request = pending_requests.front();
            // go to dest cache or directory            
               node_requests[current_request->dest_mid.nodeID].push_back(current_request);
            

	    pending_requests.pop_front();
	    
	}
}

bool ptpNetwork::ptpNetwork_request(Mreq *request)
{
	
        pending_requests.push_back(request);
   

	return true;
}

bool ptpNetwork::ptpNetwork_pushback(Mreq *request, int nodeID)
{
        
        node_requests[nodeID].push_back(request);       
        return true;
}

Mreq* ptpNetwork::ptpNetwork_fetch(int nodeID)
{
    Mreq *request;

    if(!node_requests[nodeID].empty()){

    request = new Mreq ();
    *request = *node_requests[nodeID].front();
    node_requests[nodeID].pop_front();
    return request;
    }
    else
    {
        return NULL;
    }
}

Mreq* ptpNetwork::ptpNetwork_read(int nodeID)
{
    Mreq *request;

    if(!node_requests[nodeID].empty()){

    request = new Mreq ();
    *request = *node_requests[nodeID].front();
    return request;
    }
    else
    {
        return NULL;
    }
}

void ptpNetwork::ptpNetwork_pop(int nodeID)
{

    if(!node_requests[nodeID].empty()){
    node_requests[nodeID].pop_front();
    }
}
