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
	local_id s_process_count;
	local_id s_current_id;

    timestamp_t logicTime;
    bool mutex;
    local_id s_sender_id;
    //balance_t s_balance;
	Pipes *s_pipes[MAX_PROCESS_COUNT][MAX_PROCESS_COUNT];
} Info;

typedef struct {
    local_id procId[MAX_PROCESS_ID];
    int length;
} Workers;

int receive_multicast(void * self);
int sendToAllWorkers(Info *branchData, Message *message, Workers *workers);
int receiveFromAnyWorkers(Info *branchData, Message *message);

Message create_message(int16_t type, uint16_t payload_len, void* payload);

#endif


