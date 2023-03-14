#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "lamport.h"
#include "critical_section.h"

int send(void *self, local_id dst, const Message *msg) {
    Info *info = self;
    local_id id = info->s_current_id;

    int write_fd = info->s_pipes[id][dst]->s_mode_write;
    long bytes_written = write(write_fd, msg, msg->s_header.s_payload_len + sizeof(MessageHeader));

    return bytes_written >= 0 ? 0 : -1;
}

int send_multicast(void *self, const Message *msg) {
    Info *info = self;
    local_id id = info->s_current_id;

    local_id process_count = info->s_process_count;
    for (local_id dst = 0; dst < process_count; dst++) {
        if (dst == id) {
            continue;
        }

        if (send(info, dst, msg) != 0)
            return -1;
    }

    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    Info *info = (Info *) self;
    local_id id = info->s_current_id;

    int read_fd = info->s_pipes[id][from]->s_mode_read;
    while (1) {
        long bytes_read = read(read_fd, &(msg->s_header), sizeof(MessageHeader));

        if (bytes_read > 0) {
            bytes_read = read(read_fd, &(msg->s_payload), msg->s_header.s_payload_len);

            if (info->logicTime < msg->s_header.s_local_time) {
                setLamportTime(msg->s_header.s_local_time);
            }
            incrementLamportTime();

            info->s_sender_id = from;

            return bytes_read >= 0 ? 0 : -1;
        }
    }
}

int receive_from_worker(void *self, local_id from, Message *msg) {
    Info *info = (Info *) self;
    local_id id = info->s_current_id;

    int read_fd = info->s_pipes[id][from]->s_mode_read;
    long bytes_read = read(read_fd, &(msg->s_header), sizeof(MessageHeader));

    if (bytes_read > 0) {
        bytes_read = read(read_fd, &(msg->s_payload), msg->s_header.s_payload_len);

        if (info->logicTime < msg->s_header.s_local_time) {
            setLamportTime(msg->s_header.s_local_time);
        }
        incrementLamportTime();

        info->s_sender_id = from;

        return bytes_read >= 0 ? 0 : -1;
    }

    return -1;
}

//Critical_section
int receive_any(void *self, Message *msg) {
    Info *info = (Info *) self;
    local_id id = info->s_current_id;

    local_id process_count = info->s_process_count;
    while (1) {
        for (int from = 0; from < process_count; from++) {
            if (from == id) {
                continue;
            }

            int read_fd = info->s_pipes[id][from]->s_mode_read;

            {
                long bytes_read = read(read_fd, &(msg->s_header), sizeof(MessageHeader));

                if (bytes_read > 0) {
                    bytes_read = read(read_fd, &(msg->s_payload), msg->s_header.s_payload_len);

                    if (info->logicTime < msg->s_header.s_local_time) {
                        setLamportTime(msg->s_header.s_local_time);
                    }
                    incrementLamportTime();

                    info->s_sender_id = from;

                    return bytes_read >= 0 ? 0 : -1;
                }
            }
        }
    }
}

//work_manager - wait until receive from all other subprocesses
int receive_multicast(void *self, Workers *workers, MessageType type) {
    Info *info = self;
    local_id id = info->s_current_id;

    local_id process_count = info->s_process_count;
    for (local_id i = 0; i < workers->length; i++) {
        local_id from = workers->procId[i];
        if (from == id) {
            continue;
        }

        Message message;

        if (receive(info, from, &message) != 0) {
            return -1;
        } else if (message.s_header.s_type != type) {
            i--;
            printf("%1d recieved msg from %1d with status %d\n", id, from, message.s_header.s_type);
        }
    }

    return 0;
}

int sendToAllWorkers(Info *branchData, Message *message, Workers *workers) {
    for (int i = 0; i < workers->length; ++i) {
        if (workers->procId[i] != branchData->s_current_id) {
            int result = send(branchData, workers->procId[i], message);
            if (result == -1) {
                printf("fail to send to all workers child: to %d from %d\n", workers->procId[i], branchData->s_current_id);
                return -1;
            }
        }
    }
    return 0;
}

int receiveFromAnyWorkers(Info *branchData, Message *message) {
    for (int i = 0; i < getWorkers().length; ++i) {
        if (getWorkers().procId[i] != branchData->s_current_id) {
            int result = receive_from_worker(branchData, getWorkers().procId[i], message);
            if (result == 0) {
                return 0;
            }
        }
    }
    return -1;
}

void syncReceiveDoneFromAllWorkers(void *self, Message message[], Workers *workers) {
    Info *branchData = self;
    for (int i = branchData->s_current_id+1; i < branchData->s_process_count; ++i) {
        if (i != branchData->s_current_id) {
            while (1) {
                if (receive(branchData, i, &message[i]) == 0) {
                    if (message[i].s_header.s_type == DONE) {
                        break;
                    }
                }
            }
        }
    }
}

Message create_message(int16_t type, uint16_t payload_len, void* payload) {
    MessageHeader header = {
            MESSAGE_MAGIC,
            payload_len,
            type,
            get_lamport_time()
    };

    Message msg = {header};
    if (payload != NULL)
        memcpy(&msg.s_payload, payload, payload_len);

    return msg;
}
