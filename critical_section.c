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

void check_status(Info *branchData, int ackNeeded) {
    Message message;
    local_id ackCounter = 0;

    do {
        receive_any(branchData, &message);
        switch (message.s_header.s_type) {
            case CS_REPLY: {
                ackCounter++;
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
                dequeue();
                break;
            }
            case DONE: {
                deleteWorker(branchData->s_sender_id);
                ackCounter++;
                break;
            }
            default:
                printf("%s\n", "default");
        }
    } while (ackCounter < ackNeeded);
}

int request_cs(const void * self) {
    Info *branchData = (Info*)self;

    Message requestCsMsg = create_message(CS_REQUEST, 0, NULL);
    send_multicast(branchData, &requestCsMsg);
    Request currentRequest = {requestCsMsg.s_header.s_local_time, branchData->s_current_id};
    enqueue(currentRequest);

    int workersCount = getWorkers().length;
    if (workersCount > 1) {
        check_status(branchData, workersCount - 1); // wait ack
    }

    while (compare(peek(), currentRequest) != 0) {
        check_status(branchData, 0);
    }
    return 0;
}

int release_cs(const void * self) {
    Info *branchData = (Info*)self;

    Message releaseMsg = create_message(CS_RELEASE, 0, NULL);
    send_multicast(branchData, &releaseMsg);

    dequeue();

    return 0;
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
