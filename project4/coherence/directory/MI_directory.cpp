#include "dir_controller.hpp"
#include "sim.hpp"

void Directory_controller::MI_tick() {
    Mreq *request;
    Dir_entry *entry;

    if (!request_in_progress) {
        if ((request = read_input_port()) != NULL) {
            entry = dir_table->get_entry(request->addr);
            
            if ((entry->entry_state == I) && (request->msg == GETM)) {
                /* invalid and write miss */
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = M; 
                entry->sharers[entry->share_num] = request->src_mid;
                entry->share_num++;
                pop_input_port();
            } else if ((entry->entry_state == MM) && (request->msg == DATA)) {
                /* transient state */
                data_addr = request->addr;
                data_target = entry->reqID_in_transient;
                Mreq *new_request;
                new_request = new Mreq(DATA, data_addr, moduleID, data_target);
                this->write_output_port(new_request);
                entry->entry_state = M;
                entry->sharers[entry->share_num - 1] = entry->reqID_in_transient;
                pop_input_port();
            } else if ((entry->entry_state == M) && (request->msg == GETM)) {
                /* modified and write miss */
                for (int i = 0; i < entry->share_num; i++) {
                    if (entry->sharers[i] != request->src_mid) {
                        data_addr = request->addr;
                        data_target = entry->sharers[i];
                        entry->inv_sent++;
                        Mreq *new_request;
                        new_request = new Mreq(MREQ_INVALID, data_addr, moduleID, data_target);
                        this->write_output_port(new_request);
                    } else {
                        entry->GETM_nodata = true;
                    }
                }
                entry->reqID_in_transient = request->src_mid;
                entry->entry_state = MM;
                pop_input_port();
            } else if ((entry->entry_state == MM) && (request->msg == GETM)) {
                /* transient state */
                request = fetch_input_port();
                pushback_input_port(request);
            } else {
                printf("**** fata_err --entry->entry_state: %d  ;addr: 0x%8lx; Clock: %lu \n", entry->entry_state, request->addr, Global_Clock);
                fatal_error("Memory controller miss case!!!!\n");
            }
        }
    }

    if (request_in_progress && Global_Clock >= data_time) {
        /* send out new data request */
        Mreq *new_request;
        new_request = new Mreq(DATA, data_addr, moduleID, data_target);
        request_in_progress = false;
        this->write_output_port(new_request);
    }
}

