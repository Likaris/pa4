#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "main.h"

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
            return bytes_read >= 0 ? 0 : -1;
        }
    }
}

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
                    return bytes_read >= 0 ? 0 : -1;
                }
            }
        }
    }
}

int receive_multicast(void *self) {
    Info *info = self;
    local_id id = info->s_current_id;

    local_id process_count = info->s_process_count;
    for (local_id from = 1; from < process_count; from++) {
        if (from == id){
            continue;
        }

        Message message;
        if (receive(info, from, &message) != 0)
            return -1;
    }

    return 0;
}

Message create_message(int16_t type, uint16_t payload_len, void* payload) {
    MessageHeader header = {
            MESSAGE_MAGIC,
            payload_len,
            type,
            get_physical_time()
    };

    Message msg = {header};
    if (payload != NULL)
        memcpy(&msg.s_payload, payload, payload_len);

    return msg;
}
