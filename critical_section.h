//
// Created by alika on 13.03.23.
//

#ifndef PA4_CRITICAL_SECTION_H
#define PA4_CRITICAL_SECTION_H

#include "ipc.h"
#include "main.h"
#include <stdbool.h>


Workers getWorkers();

void initWorkers(int);
void deleteWorker(local_id id);


typedef struct {
    timestamp_t time;
    local_id procId;
} Request;

void syncReceiveCs(Info *branchData);
Request sendAndSaveCsRequest(Info *branchData);
void receiveAllRepliesHandler(Info *branchData, Request currentRequest, Workers workers);
int checkEnterCondition(Info *branchData, Request currentRequest);
void receiveCsRequestAndSendReply(Info *branchData, Message);
void receiveCsRelease(Info *branchData, Message release);



#define MAX_QUEUE_SIZE 2048

typedef struct {
    Request requests[MAX_QUEUE_SIZE];
    int length;
} Queue;

bool enqueue(Request request);

Request dequeue();

Request peek();

int compare(Request, Request);

#endif //PA4_CRITICAL_SECTION_H
