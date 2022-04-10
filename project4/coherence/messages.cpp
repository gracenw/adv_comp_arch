/*
 * messages.cpp
 *
 *  Created on: Apr 8, 2011
 *      Author: japoovey
 */
#include "messages.hpp"
#include "mreq.hpp"

/** This contains the message types as strings.  This must be of the same length
 * and the same order as the enum in messages.h.  This is used to output the message
 * trace.  This is needed to match the validation runs.
 */

const char * Mreq::message_t_str[MREQ_MESSAGE_NUM] = {

    "NOP",

    "LOAD",
    "STORE",

    "GETS",
    "GETM",

    "DATA",

    "MREQ_INVALID",

    "INVACK",
    "UPGRADE_M",

    "DATA_E",

    "SILENT_UPGRADE_EM",

    "MREQ_INVALID_NOTRANSFER",
    "DATA_F"
};
