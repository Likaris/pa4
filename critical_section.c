//
// Created by alika on 13.03.23.
//

#include <stdio.h>
#include "critical_section.h"
#include "lamport.h"

Workers _workers;

void initWorkers(int workerCount) {
    _workers.length = workerCount - 1;
    for (int i = 0; i < _workers.length; ++i) {
        _workers.procId[i] = i + 1;
    }
}

void deleteWorker(local_id id) {
    for (int i = 0; i <_workers.length; ++i) {
        if (_workers.procId[i] == id) {
            for (int j = i; j <_workers.length - 1; ++j) {
                _workers.procId[j] = _workers.procId[j + 1];
            }
            break;
        }
    }
    _workers.length--;
}

Workers getWorkers() {
    return _workers;
}

void printWorkers() {
    printf("[ ");
    for (int i = 0; i <_workers.length; ++i) {
        printf("%d, ", _workers.procId[i]);
    }
    printf(" ]\n");
}

int request_cs(const void * self) {
    Info *branchData = (Info*)self;

    //printf("proc %d tries to request\n", branchData->s_current_id);
    //fflush(stdout);

    Request currentRequest = sendAndSaveCsRequest(branchData);

    if (getWorkers().length > 1) {
        Workers workers = getWorkers();
        receiveAllRepliesHandler(branchData, currentRequest, workers);
        //printf("proc %d received all approves\n", branchData->s_current_id);
        //fflush(stdout);
    }



    while (checkEnterCondition(branchData, currentRequest) != 0) {
        //printf("proc %d wait for approve\n", branchData->s_current_id);
        syncReceiveCs(branchData);
    }
    return 0;
}

int release_cs(const void * self) {
    Info *branchData = (Info*)self;
    dequeue();
    //printf("in proc %d request (%d, %d) was deleted\n", branchData->id, request.time, request.procId);
    fflush(stdout);

    incrementLamportTime();
    Message releaseMsg = create_message(CS_RELEASE, 0, NULL);
    //Message releaseMsg;
    //buildCsMessage(&releaseMsg, CS_RELEASE);
    Workers workers = getWorkers();

    //TODO
//    for (int i = 0; i < workers.length; ++i) {
//        if (workers.procId[i] != branchData->s_current_id) {
//            int result = send(branchData, workers.procId[i], &releaseMsg);
//            if (result == -1) {
//                printf("fail to send to all workers child\n");
//                return -1;
//            }
//        }
//    }

    sendToAllWorkers(branchData, &releaseMsg, &workers);
    //printf("in proc %d send to delete (%d, %d)\n", branchData->id, request.time, request.procId);
    fflush(stdout);
    return 0;
}



void syncReceiveCs(Info *branchData) {
    Message requestFromOther;
    while (true) {
        if (receive_any(branchData, &requestFromOther) == 0) {
            if (requestFromOther.s_header.s_type == CS_REQUEST) {
                receiveCsRequestAndSendReply(branchData, requestFromOther);
            } else if (requestFromOther.s_header.s_type == CS_RELEASE) {
                receiveCsRelease(branchData, requestFromOther);
            } else if (requestFromOther.s_header.s_type == DONE) {
                deleteWorker(branchData->s_sender_id);
                //printf("in proc %d delete worker %d\n", branchData->id, branchData->senderId);
                fflush(stdout);
            } else {
                //printf("smth wrong\n");
                fflush(stdout);
            }
            return;
        }
    }
}

Request sendAndSaveCsRequest(Info *branchData) {
    incrementLamportTime();

    Message requestCsMsg = create_message(CS_REQUEST, 0, NULL);
    //Message requestCsMsg;
    //buildCsMessage(&requestCsMsg, CS_REQUEST);
    Workers workers = getWorkers();
    //TODO
    //printf("proc %d send request (%d, %d)\n", branchData->s_current_id, get_lamport_time(), branchData->s_current_id);
    //fflush(stdout);
    sendToAllWorkers(branchData, &requestCsMsg, &workers);


//    for (int i = 0; i < workers.length; ++i) {
//        if (workers.procId[i] != branchData->s_current_id) {
//            int result = send(branchData, workers.procId[i], &requestCsMsg);
//        }
//    }


    Request request = {get_lamport_time(), branchData->s_current_id};
    enqueue(request);
    //printf("proc %d enqueue request (%d, %d)\n", branchData->s_current_id, request.time, request.procId);
    //fflush(stdout);
    return request;
}



void receiveAllRepliesHandler(Info *branchData, Request currentRequest, Workers workers) {
    Message csReplies;
    local_id ackCounter = 0;
    int currentWorkersLength = getWorkers().length - 1;


    while (ackCounter < currentWorkersLength) {
        if (receiveFromAnyWorkers(branchData, &csReplies) == 0) {
            if (csReplies.s_header.s_type == CS_REPLY) {
                ackCounter++;
//                printf("proc %d RECEIVED from request %d: Reply; ack = %d\n", branchData->s_current_id, branchData->s_sender_id, ackCounter);
//                fflush(stdout);
            } else if (csReplies.s_header.s_type == CS_REQUEST) {
//                printf("proc %d RECEIVED from request %d: Request\n", branchData->s_current_id, branchData->s_sender_id);
//                fflush(stdout);
                receiveCsRequestAndSendReply(branchData, csReplies);
            } else if (csReplies.s_header.s_type == CS_RELEASE) {
//                printf("proc %d RECEIVED from request %d: Release\n", branchData->s_current_id, branchData->s_sender_id);
//                fflush(stdout);
                receiveCsRelease(branchData, csReplies);
            } else if (csReplies.s_header.s_type == DONE) {
//                printf("proc %d RECEIVED from request %d: Done\n", branchData->s_current_id, branchData->s_sender_id);
//                fflush(stdout);

                deleteWorker(branchData->s_sender_id);
                ackCounter++;
            } else {
                printf("smth wrong\n");
            }
        }
    }
}

int checkEnterCondition(Info *branchData, Request currentRequest) {

    Request firstRequest = peek();

    if (compare(firstRequest, currentRequest) == 0) {
        //printf("in proc %d approved the request (%d, %d)\n", branchData->id, currentRequest.time, currentRequest.procId);
        fflush(stdout);
        return 0;
    } else {
        //printf("in proc %d unapproved the request (%d, %d)\n", branchData->id, currentRequest.time, currentRequest.procId);
        fflush(stdout);
        return -1;
    }
}

void receiveCsRequestAndSendReply(Info *branchData, Message requestFromOther) {
    Request request = {requestFromOther.s_header.s_local_time, branchData->s_sender_id};
    //printf("proc %d receive req (%d, %d) from proc %d\n",
    //        branchData->id, request.time, request.procId,  branchData->senderId);
    fflush(stdout);

    enqueue(request);
    //printf("proc %d enqueue request (%d, %d)\n", branchData->id, request.time, request.procId);
    fflush(stdout);

    incrementLamportTime();

    Message replyMsg = create_message(CS_REPLY, 0, NULL);

//    Message replyMsg;
//    buildCsMessage(&replyMsg, CS_REPLY);
    //printf("proc %d send reply for (%d, %d) to proc %d\n", branchData->s_current_id, request.time, request.procId, branchData->s_sender_id);
    send(branchData, branchData->s_sender_id, &replyMsg);
    //fflush(stdout);
}

void receiveCsRelease(Info *branchData, Message release) {
    //Request request = {release.s_header.s_local_time, branchData->senderId};

    //printf("proc %d receive release msg from proc %d\n", branchData->id, branchData->senderId);
    fflush(stdout);
    dequeue(); // тут нужно удалить именно тот, который пришел, а не первый
    //printf("proc %d delete request (%d, %d)\n", branchData->id, request.time, request.procId);
    fflush(stdout);
}

Queue queue;

bool isFull() {
    return queue.length == MAX_QUEUE_SIZE;
}

bool isEmpty() {
    return queue.length == 0;
}

// если первый больше второго возвращает 1, если второй больше первого возвращает -1
int compare(Request request1, Request request2) {
    if (request1.time < request2.time) {
        return 1;
    }
    if (request1.time == request2.time) {
        if (request1.procId < request2.procId) {
            return 1;
        } else if (request1.procId == request2.procId) {
            return 0;
        }
    }
    return -1;
}

// находит id максимального элемента в очереди по компаратору
int getMaxRequestId() {
    if (queue.length <= 0) {
        return -1;
    } else {
        int getMaxRequestId = 0;
        for (int i = 1; i < queue.length; ++i) {
            if (compare(queue.requests[i], queue.requests[getMaxRequestId]) > 0) { // если первый больше,
                getMaxRequestId = i;
            }
        }
        return getMaxRequestId;
    }
}

bool enqueue(Request request) {
    if (!isFull()) {
        queue.requests[queue.length] = request;
        queue.length++;
        return true;
    } else {
        printf("attempt to enqueue to full queue\n");
        return false;
    }
}

// вытаскивает первый элемент из очереди
Request dequeue() {
    if (!isEmpty()) {
        local_id maxRequestId = getMaxRequestId();
        Request maxRequest = queue.requests[maxRequestId];
        queue.requests[maxRequestId] = queue.requests[queue.length - 1];
        queue.length--;
        return maxRequest;
    } else {
        printf("attempt to dequeue from empty queue\n");
        Request request = { -1, -1};
        return request;
    }
}

// возвраает первый элемент их очереди без его удаления из очереди
Request peek() {
    if (!isEmpty()) {
        int maxRequestId = getMaxRequestId();
        return queue.requests[maxRequestId];
    } else {
        printf("attempt to pick from empty queue\n");
        Request request = { -1, -1};
        return request;
    }
}
