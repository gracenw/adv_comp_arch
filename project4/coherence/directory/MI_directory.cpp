#include "dir_controller.hpp"
#include "sim.hpp"

void Directory_controller::MI_tick() {
    Mreq *request;
    Dir_entry *entry;

    if (!request_in_progress) {
        if ((request = read_input_port()) != NULL) {
            entry = dir_table->get_entry(request->addr);

            if ((entry->entry_state == I) && (request->msg == GETM)) {
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = M;
                
                Mreq *new_request;
                new_request = new Mreq(MREQ_INVALID, data_addr, moduleID, data_target);
                this->write_output_port(new_request);

                pop_input_port();
            }
            else if ((entry->entry_state == I) && (request->msg == GETS)) {
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = M;
                pop_input_port();
            }
            else if ((entry->entry_state == M) && (request->msg == GETM)) {
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = M;
                pop_input_port();
            }
            else if ((entry->entry_state == M) && (request->msg == GETS)) {
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = M;
                pop_input_port();
            }
            else {
                printf("**** fata_err --entry->entry_state: %d  ;addr: 0x%8lx; Clock: %lu \n", entry->entry_state, request->addr, Global_Clock);
                fatal_error("Memory controller miss case!!!!\n");
            }
        }
    }

    if (request_in_progress && Global_Clock >= data_time) {
        Mreq *new_request;
        new_request = new Mreq(DATA, data_addr, moduleID, data_target);
        request_in_progress = false;
        this->write_output_port(new_request);
    }
}

