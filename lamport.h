//
// Created by alika on 13.03.23.
//

#ifndef PA2_LAMPORT_H
#define PA2_LAMPORT_H

#include "ipc.h"

timestamp_t get_lamport_time();
void setLamportTime(timestamp_t);

void incrementLamportTime();

#endif //PA2_LAMPORT_H
