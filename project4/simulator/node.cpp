#include "node.hpp"
#include "processor.hpp"
#include "hash_table.hpp"
#include "dir_controller.hpp"
#include "sim.hpp"

Node::Node (int nodeID)
{
    this->nodeID = nodeID;
    mod[L1_M] = NULL;
    mod[PR_M] = NULL;
    mod[DC_M] = NULL;
}

Node::~Node ()
{
    map<module_t, Module*>::iterator it;
    for (it = mod.begin (); it != mod.end (); it++)
        delete (it->second);
    mod.clear ();
}

void Node::build_processor (char *trace_file)
{
    Hash_table *cache;

    mod[L1_M] = cache = new Hash_table ({nodeID, L1_M}, "L1", 
                                        settings.l1_cache_size,
                                        settings.l1_cache_assoc,
                                        settings.cache_line_size,
                                        settings.l1_mshrs,
                                        settings.l1_hit_time,
                                        settings.protocol);

    mod[PR_M] = new Processor ({nodeID, PR_M}, cache, trace_file);
}

void Node::build_directory_controller (void)
{
	mod[DC_M] = new Directory_controller ({nodeID, DC_M}, 100);
}

void Node::tick_cache (void)
{
	if (mod[L1_M])
		mod[L1_M]->tick ();
}

void Node::tick_pr (void)
{
	if (mod[PR_M])
		mod[PR_M]->tick ();
}

void Node::tick_dc (void)
{
	if (mod[DC_M])
		mod[DC_M]->tick ();
}

void Node::tock_pr (void)
{
	if (mod[PR_M])
		mod[PR_M]->tock ();
}
