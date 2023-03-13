#ifndef PA2_WORKMANAGER_H
#define PA2_WORKMANAGER_H

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>

#include "common.h"
#include "pa2345.h"
#include "main.h"
#include "lamport.h"

void init_topology(Info* info);

void open_pipes(Info* info);

pid_t* fork_processes(local_id process_count, Info* info, FILE * events_file_ptr);

void close_pipes(Info* info, bool owned_only);

void do_work(local_id process_count);

#endif

