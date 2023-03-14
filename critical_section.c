//
// Created by alika on 13.03.23.
//

#include <stdio.h>
#include "critical_section.h"
#include "lamport.h"

timestamp_t queue[32];

void init_queue(Info* info) {
    for (size_t i = 0; i < info->s_process_count - 1; i++) {
        queue[i] = INT16_MAX;
    }
}

void enqueue(Request request) {
    queue[request.procId] = request.time;
}

int peek(Info* info) {
    timestamp_t minimum = INT16_MAX;
    int pos = 0;

    for (int i = 0; i < info->s_process_count; i++) {
        if (queue[i] > -1 && queue[i] < minimum) {
            minimum = queue[i];
            pos = i;
        }
    }

    return pos;
}

void dequeue(Info* info) {
    int pos = peek(info);
    queue[pos] = INT16_MAX;
}

timestamp_t* get_queue() {
    return queue;
}

void check_status(Info *branchData, int ackNeeded) {
    Message message;

    do {
        receive_any(branchData, &message);
        switch (message.s_header.s_type) {
            case CS_REPLY: {
                ackNeeded--;
                break;
            }
            case CS_REQUEST: {
                Message replyMsg = create_message(CS_REPLY, 0, NULL);
                send(branchData, branchData->s_sender_id, &replyMsg);
                Request request = {message.s_header.s_local_time, branchData->s_sender_id};
                enqueue(request);
                break;
            }
            case CS_RELEASE: {
                dequeue(branchData);
                break;
            }
            case DONE: {
                queue[branchData->s_sender_id - 1] = -1;
                //deleteWorker(branchData->s_sender_id);
                ackNeeded--;
                break;
            }
            default:
                printf("%s\n", "default");
        }
    } while (ackNeeded > 0);
}

int request_cs(const void * self) {
    Info *branchData = (Info*)self;

    Message requestCsMsg = create_message(CS_REQUEST, 0, NULL);
    send_multicast(branchData, &requestCsMsg);
    Request currentRequest = {requestCsMsg.s_header.s_local_time, branchData->s_current_id};
    enqueue(currentRequest);

    int workersCount = 0;
    for (int i = 1; i < branchData->s_process_count; i++) {
        if (queue[i-1] != -1) {
            workersCount ++;
        }
    }

    if (workersCount > 1) {
        check_status(branchData, workersCount - 1); // wait ack
    }

    while (peek(branchData) != branchData->s_current_id) {
        check_status(branchData, 0);
    }
    return 0;
}

int release_cs(const void * self) {
    Info *branchData = (Info*)self;

    Message releaseMsg = create_message(CS_RELEASE, 0, NULL);
    send_multicast(branchData, &releaseMsg);

    dequeue(branchData);

    return 0;
}

