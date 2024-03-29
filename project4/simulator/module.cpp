#include <assert.h>
#include <string.h>
#include "module.hpp"
#include "mreq.hpp"
#include "sim.hpp"
#include "types.hpp"

bool ModuleID::operator== (const ModuleID &mid)
{
    return (this->nodeID == mid.nodeID &&
            this->module_index == mid.module_index);
}

bool ModuleID::operator!= (const ModuleID &mid)
{
    return !(this->nodeID == mid.nodeID &&
             this->module_index == mid.module_index);
}

Module *ModuleID::get_module ()
{
    Module *ret = Sim->Nd[nodeID]->mod[module_index];
    assert (ret != NULL);
    return ret;
}

/************************************************************
 * Module constructor, destructor, and associated functions.
 ************************************************************/
Module::Module (ModuleID moduleID, const char *name)
{
    this->moduleID = moduleID;
    this->name = strdup (name);
}

Module::~Module (void)
{
    if (name)
        free (name);
}

Mreq *Module::fetch_input_port (void)
{
    return Sim->ptpnetwork->ptpNetwork_fetch(moduleID.nodeID);
}

Mreq *Module::read_input_port (void)
{
    return Sim->ptpnetwork->ptpNetwork_read(moduleID.nodeID);
}

void Module::pop_input_port (void)
{
    Sim->ptpnetwork->ptpNetwork_pop(moduleID.nodeID);
}

bool Module::write_output_port (Mreq *mreq)
{
    return Sim->ptpnetwork->ptpNetwork_request (mreq);
}

bool Module:: pushback_input_port (Mreq *mreq){

return Sim->ptpnetwork->ptpNetwork_pushback(mreq, moduleID.nodeID);
}

void print_id (const char *str, ModuleID mid)
{
    switch (mid.module_index) {
    case NI_M: printf("%4s:%3d/NI  ", str, mid.nodeID); break;
    case PR_M: printf("%4s:%3d/PR  ", str, mid.nodeID); break;
    case L1_M: printf("%4s:%3d/L1  ", str, mid.nodeID); break;
    case L2_M: printf("%4s:%3d/L2  ", str, mid.nodeID); break;
    case L3_M: printf("%4s:%3d/L3  ", str, mid.nodeID); break;
    case DC_M: printf("%4s:%3d/DC  ", str, mid.nodeID); break;
    case INVALID_M:  printf("%4s:  None ", str); break;
    }
}

