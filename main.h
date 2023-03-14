#ifndef PA2_MAIN_H
#define PA2_MAIN_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "ipc.h"

#define MAX_PROCESS_COUNT 11

typedef struct{
	int s_mode_read;
	int s_mode_write;
} Pipes;

typedef struct{
	local_id s_process_count; //1 parent + p children
    timestamp_t logicTime;
    bool mutex;

    local_id s_current_id;
    local_id s_sender_id;

	Pipes *s_pipes[MAX_PROCESS_COUNT][MAX_PROCESS_COUNT];
} Info;

int receive_multicast(void * self, MessageType type);
Message create_message(int16_t type, uint16_t payload_len, void* payload);

#endif


