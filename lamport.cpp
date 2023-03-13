//
// Created by alika on 13.03.23.
//

#include "lamport.h"

timestamp_t lamportTime = 0;

timestamp_t get_lamport_time() {
    return lamportTime;
}

void setLamportTime(timestamp_t timestamp) {
    lamportTime = timestamp;
}

void incrementLamportTime() {
    lamportTime++;
}



