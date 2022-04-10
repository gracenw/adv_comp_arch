#ifndef NODE_H_
#define NODE_H_

#include "types.hpp"
#include "module.hpp"

using namespace std;

class Network_interface;
class Predictor;

class Node
{
public:
    int nodeID;
    map<module_t, Module*> mod;

    Node (int nodeID);
    virtual ~Node ();

    Predictor *predictor;

    void build_processor (char *trace_file);
    void build_directory_controller (void);
    
    void tick_cache (void);
    void tick_pr (void);
    void tick_dc (void);
    void tock_pr (void);
};

#endif /* NODE_H_ */
