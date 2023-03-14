#include "work_manager.h"

char * const init_pipe_msg = "Pair connection, %d -> %d created\n";

static const char * const opened_pipe_msg = "Pipe for current process%d from process %d to %d with descriptor %d opened\n";
static const char * const closed_pipe_msg = "Pipe for current process%d from process %d to %d with descriptor %d closed\n";

FILE *pipes_file_ptr;

void init_topology(Info *info) {
    local_id process_count = info->s_process_count;
    for (local_id i = 0; i < process_count; i++) {
        for (local_id j = 0; j < process_count; j++) {
            if (i != j) {
                Pipes *pipeFd = malloc(sizeof(Pipes));
                info->s_pipes[i][j] = pipeFd;

                fprintf(pipes_file_ptr, init_pipe_msg, i, j);
            }
        }
    }
}

void open_pipes(Info *info) {
    local_id process_count = info->s_process_count;
    int modes[2];
    char buffer[100];

    for (local_id from = 0; from < process_count; from++) {
        for (local_id dst = 0; dst < process_count; dst++) {
            if (from != dst) {
                pipe2(modes, O_NONBLOCK);
                int read_end = modes[0];
                int write_end = modes[1];

                info->s_pipes[dst][from]->s_mode_read = read_end;
                info->s_pipes[from][dst]->s_mode_write = write_end;

                sprintf(buffer, opened_pipe_msg, 0, dst, from, read_end);
                fprintf(pipes_file_ptr, "%s", buffer);

                printf(buffer, opened_pipe_msg, 0, from, dst, write_end);
                fprintf(pipes_file_ptr, "%s", buffer);
            }
        }
    }
}

//owned_only == false ~ all but owned
void close_pipes(Info *info, bool owned_only) {
    local_id id = info->s_current_id;
    local_id process_count = info->s_process_count;
    Pipes *pipe_fd;
    char buffer[100];

    for (local_id from = 0; from < process_count; from++) {
        if (owned_only && from != id) continue;
        if (!owned_only && from == id) continue;
        for (int dst = 0; dst < process_count; dst++) {
            if (from != dst) {
                pipe_fd = info->s_pipes[from][dst];

                sprintf(buffer, closed_pipe_msg, id, from, dst, pipe_fd->s_mode_write);
                fprintf(pipes_file_ptr, "%s", buffer);

                close(pipe_fd->s_mode_write);

                sprintf(buffer, closed_pipe_msg, id, from, dst, pipe_fd->s_mode_read);
                fprintf(pipes_file_ptr, "%s", buffer);

                close(pipe_fd->s_mode_read);
            }
        }
    }
}

void wait_other_start(Info *info, FILE * events_file_ptr) {
    local_id id = info->s_current_id;

    incrementLamportTime();
    Message msg = create_message(STARTED, 0, NULL);
    fprintf(events_file_ptr, log_started_fmt, msg.s_header.s_local_time, id, getpid(), getppid(), 0);
    fflush(events_file_ptr);

    if (send_multicast(info, &msg) != 0) {
        exit(1);
    }


    Workers workers = getWorkers();
    printf("%d: waiting for %d procs\n", id, workers.length);

    receive_multicast(info, &workers, STARTED);

    fprintf(events_file_ptr, log_received_all_started_fmt, get_lamport_time(), id);
    fflush(events_file_ptr);
}

void process_stop_msg(Info *info, FILE * events_file_ptr) {
    incrementLamportTime();
    Message reply = create_message(DONE, 0, NULL);
    fprintf(events_file_ptr, log_done_fmt, reply.s_header.s_local_time, info->s_current_id, 0);
    fflush(events_file_ptr);

    send_multicast(info, &reply);
}

void process_done_msg(Info *info, FILE * events_file_ptr) {
    //local_id id = info->s_current_id;
    //fprintf(events_file_ptr, log_received_all_done_fmt, history->s_history_len, id);

    Message msg = create_message(BALANCE_HISTORY, 0, NULL);
    send(info, PARENT_ID, &msg);
}


void do_payload(Info *info, FILE * events_file_ptr) {

    local_id id = info->s_current_id;
    int loopCount = id * 5;

    if (info->mutex) {
        for (int i = 1; i <= loopCount; ++i) {
            char message[256];
            sprintf(message, log_loop_operation_fmt, info->s_current_id, i, loopCount);
            request_cs(info);
            print(message); //TODO: teacher print
            //printf(message); //TODO: hide
            release_cs(info);
        }
    } else {
        for (int i = 1; i <= loopCount; ++i) {
            char message[256];
            sprintf(message, log_loop_operation_fmt, info->s_current_id, i, loopCount);
            print(message); //TODO: teacher print
        }
    }

    //Message doneMessages[info->s_process_count];
    Workers workers = getWorkers();
    process_stop_msg(info, events_file_ptr);
    //syncReceiveDoneFromAllWorkers(info, doneMessages, &workers);

    receive_multicast(info, &workers,DONE); //All DONE
    printf("closing proc: %d", id);

}

void pipe_work(local_id id, Info *info, FILE * events_file_ptr) {
    info->s_current_id = id;
    //info->s_balance = start_balance;

    close_pipes(info, false);
    wait_other_start(info, events_file_ptr);
    do_payload(info, events_file_ptr);
    close_pipes(info, true);
}

pid_t *fork_processes(local_id process_count, Info *info, FILE * events_file_ptr) {
    pid_t *all_pids = malloc(sizeof(pid_t) * MAX_PROCESS_COUNT);
    all_pids[0] = getpid();
    for (local_id child_id = 1; child_id < process_count; child_id++) {
        all_pids[child_id] = fork();
        if (all_pids[child_id] == 0) {
            //continue in created child process
            pipe_work(child_id, info, events_file_ptr);
            exit(0);
        }
    }
    return all_pids;
}

void parent_work(Info *info) {
    info->s_current_id = PARENT_ID;
    //local_id child_count = info->s_process_count - 1;

    close_pipes(info, false);

    incrementLamportTime();
    Workers workers = getWorkers();
    printf("%d: waiting for %d procs\n", 0, workers.length);

    receive_multicast(info, &workers, STARTED); //All STARTED

    //bank_robbery(info, child_count);

//    Message msg = create_message(STOP, 0, NULL);
//    if (send_multicast(info, &msg) != 0) {
//        exit(1);
//    }

    //workers = getWorkers();
    receive_multicast(info, &workers, DONE); //All DONE

    while (wait(NULL) > 0) {
    }

    close_pipes(info, true);
}


void do_work(local_id process_count, bool mutex) {
    Info *info = malloc(sizeof(Info));
    info->s_process_count = process_count;
    info->logicTime = get_lamport_time();
    info->mutex = mutex; //TODO: input
    initWorkers(info->s_process_count);

    FILE *events_file_ptr = fopen(events_log, "a");
    pipes_file_ptr = fopen(pipes_log, "a");

    init_topology(info);
    open_pipes(info);
    fork_processes(process_count, info, events_file_ptr);
    parent_work(info);

    fclose(events_file_ptr);
    fclose(pipes_file_ptr);
}

