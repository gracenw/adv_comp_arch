#include "dir_controller.hpp"
#include "sim.hpp"

// const char * DIR_strings[19] = { "M", "O", "S", "I", "IM", "IS", "F", "FS", "FM", "SM", "MO", "SS", "MS", "MM", "OM", "E", "ES", "EM", "OO" };

void Directory_controller::MSI_tick() {
    Mreq *request;
    Dir_entry *entry;

    if (!request_in_progress) {
        if ((request = read_input_port()) != NULL) {
            entry = dir_table->get_entry(request->addr);

            if ((entry->entry_state == I) && (request->msg == GETM)) {
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = M; 
                entry->sharers[entry->share_num] = request->src_mid;
                entry->share_num++;
                pop_input_port();
            } else if ((entry->entry_state == MM) && (request->msg == DATA)) {
                data_addr = request->addr;
                data_target = entry->reqID_in_transient;
                Mreq *new_request;
                new_request = new Mreq(DATA, data_addr, moduleID, data_target);
                this->write_output_port(new_request);
                entry->entry_state = M;
                entry->sharers[entry->share_num - 1] = entry->reqID_in_transient;
                pop_input_port();
            } else if ((entry->entry_state == M) && (request->msg == GETM)) {
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
            } else if ((entry->entry_state == I) && (request->msg == GETS)) {
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                send_DATA_E = true;
                entry->entry_state = S;
                entry->sharers[entry->share_num] = request->src_mid;
                entry->share_num++;
                pop_input_port();
            } else if ((entry->entry_state == M) && (request->msg == GETS)) {
                data_addr = request->addr;
                data_target = entry->sharers[entry->share_num - 1];
                Mreq *new_request;
                new_request = new Mreq(GETS, data_addr, moduleID, data_target);
                this->write_output_port(new_request);
                entry->entry_state = MS;
                entry->reqID_in_transient = request->src_mid;
                pop_input_port();
            } else if ((entry->entry_state == MS) && (request->msg == DATA)) {
                data_addr = request->addr;
                data_target = entry->reqID_in_transient;
                Mreq *new_request;
                new_request = new Mreq(DATA, data_addr, moduleID, data_target);
                this->write_output_port(new_request);
                entry->entry_state = S;
                entry->share_num++;
                entry->sharers[entry->share_num - 1] = entry->reqID_in_transient;
                pop_input_port();
            } else if ((entry->entry_state == S) && (request->msg == GETS)) {
                request_in_progress = true;
                data_addr = request->addr;
                data_target = request->src_mid;
                data_time = Global_Clock + miss_time;
                entry->entry_state = S;
                entry->sharers[entry->share_num] = request->src_mid;
                entry->share_num++;
                pop_input_port();
            } else if ((entry->entry_state == S) && (request->msg == GETM)) {
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
                entry->entry_state = SM;
                pop_input_port();
            } else if ((entry->entry_state == SM) && (request->msg == INVACK)) {
                if (entry->inv_sent > 1) {
                    entry->inv_sent--;
                    pop_input_port();
                } else if (entry->inv_sent == 1) {
                    entry->inv_sent--;
                    if (entry->GETM_nodata == true) {
                        entry->GETM_nodata = false;
                        data_addr = request->addr;
                        data_target = entry->reqID_in_transient;
                        Mreq *new_request;
                        new_request = new Mreq(UPGRADE_M, data_addr, moduleID, data_target);
                        this->write_output_port(new_request);

                    } else {
                        request_in_progress = true;
                        data_addr = request->addr;
                        data_target = entry->reqID_in_transient;
                        data_time = Global_Clock + miss_time;
                    }
                    entry->entry_state = M;
                    entry->share_num = 1;
                    entry->sharers[entry->share_num - 1] = entry->reqID_in_transient;
                    pop_input_port();
                } else {
                    fatal_error("SM->INVACK <=0\n");
                }
            } else if (((entry->entry_state == SM) || (entry->entry_state == MM) || (entry->entry_state == MS))
                         && ((request->msg == GETM) || (request->msg == GETS))) {
                request = fetch_input_port();
                pushback_input_port(request);
            } else {
                printf("**** fata_err --entry->entry_state: %d  ;addr: 0x%8lx; Clock: %lu \n",
                        entry->entry_state, request->addr, Global_Clock);
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
