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

    Message msg = create_message(STARTED, 0, NULL);
    fprintf(events_file_ptr, log_started_fmt, msg.s_header.s_local_time, id, getpid(), getppid(), info->s_balance);

    if (send_multicast(info, &msg) != 0) {
        exit(1);
    }

    receive_multicast(info);
    fprintf(events_file_ptr, log_received_all_started_fmt, get_physical_time(), id);
}

balance_t process_transfer(Info *info, Message *msg, BalanceHistory *history, BalanceState *state,
                           FILE * events_file_ptr) {
    local_id id = info->s_current_id;
    BalanceState last_state = *state;

    TransferOrder transferOrder;
    memcpy(&transferOrder, msg->s_payload, sizeof(TransferOrder));

    if (transferOrder.s_src == id) { // this process is src
        state->s_balance -= transferOrder.s_amount;

        send(info, transferOrder.s_dst, msg);
        fprintf(events_file_ptr, log_transfer_out_fmt,
                msg->s_header.s_local_time, id, transferOrder.s_amount, transferOrder.s_dst);

    } else if (transferOrder.s_dst == id) {
        state->s_balance += transferOrder.s_amount;

        Message reply = create_message(ACK, 0, NULL);
        send(info, PARENT_ID, &reply);
        fprintf(events_file_ptr, log_transfer_in_fmt,
                msg->s_header.s_local_time, id, transferOrder.s_amount, transferOrder.s_src);
    }

    state->s_time = get_physical_time();
    history->s_history[state->s_time] = *state;
    history->s_history_len = state->s_time + 1;

    for (; last_state.s_time < state->s_time; last_state.s_time++) {
        history->s_history[last_state.s_time] = last_state;
    }

    return state->s_balance;
}

void process_stop_msg(Info *info) {
    Message reply = create_message(DONE, 0, NULL);
    send_multicast(info, &reply);
}

void process_done_msg(Info *info, BalanceHistory *history, FILE * events_file_ptr) {
    local_id id = info->s_current_id;
    fprintf(events_file_ptr, log_received_all_done_fmt, history->s_history_len, id);

    Message msg = create_message(BALANCE_HISTORY, sizeof(BalanceHistory), history);
    send(info, PARENT_ID, &msg);
}


void do_payload(Info *info, FILE * events_file_ptr) {
    local_id process_count = info->s_process_count;
    timestamp_t last_time = 0;

    BalanceState state = {info->s_balance, last_time, 0};
    BalanceHistory history = {info->s_current_id, 0};
    history.s_history[0] = state;

    int done_count = 0;

    Message message;
    while (1) {
        receive_any(info, &message);
        switch (message.s_header.s_type) {
            case TRANSFER: {
                process_transfer(info, &message, &history, &state, events_file_ptr);
                break;
            }
            case STOP: {
                process_stop_msg(info);
                break;
            }
            case DONE: {
                done_count++;
                if (done_count == process_count - 2)
                {
                    process_done_msg(info, &history, events_file_ptr);
                    return;
                }
                break;
            }
            default:
                printf("%s\n", "default");
                break;
        }
    }
}

void pipe_work(local_id id, Info *info, balance_t start_balance, FILE * events_file_ptr) {
    info->s_current_id = id;
    info->s_balance = start_balance;

    close_pipes(info, false);
    wait_other_start(info, events_file_ptr);
    do_payload(info, events_file_ptr);
    close_pipes(info, true);
}

pid_t *fork_processes(local_id process_count, Info *info, balance_t *balances, FILE * events_file_ptr) {
    pid_t *all_pids = malloc(sizeof(pid_t) * MAX_PROCESS_COUNT);
    all_pids[0] = getpid();
    for (local_id child_id = 1; child_id < process_count; child_id++) {
        all_pids[child_id] = fork();
        if (all_pids[child_id] == 0) {
            //continue in created child process
            pipe_work(child_id, info, balances[child_id - 1], events_file_ptr);
            exit(0);
        }
    }
    return all_pids;
}

void get_all_history_messages(AllHistory *all_history, Info *info) {
    local_id process_count = info->s_process_count;

    int max_history_len = 0;
    Message history_msg;
    for (local_id from = 1; from < process_count; from++) {
        receive(info, from, &history_msg);
        BalanceHistory *history = (BalanceHistory *) history_msg.s_payload;
        memcpy(&all_history->s_history[from-1], history, sizeof(BalanceHistory));

        if (history->s_history_len > max_history_len)
            max_history_len = history->s_history_len;
    }

    for (local_id history_id = 0; history_id < process_count - 1; history_id++) {
        BalanceHistory history = all_history->s_history[history_id];
        BalanceState last_state = history.s_history[history.s_history_len - 1];

        if (last_state.s_time < max_history_len) {
            for (; last_state.s_time < max_history_len; last_state.s_time++) {
                all_history->s_history[history_id].s_history[last_state.s_time] = last_state;
            }

            all_history->s_history[history_id].s_history_len = max_history_len;
        }
    }
}

void parent_work(Info *info) {
    info->s_current_id = PARENT_ID;
    local_id child_count = info->s_process_count - 1;

    close_pipes(info, false);

    receive_multicast(info); //All STARTED

    bank_robbery(info, child_count);

    Message msg = create_message(STOP, 0, NULL);
    if (send_multicast(info, &msg) != 0) {
        exit(1);
    }

    receive_multicast(info); //All DONE

    AllHistory all_history = {child_count};
    get_all_history_messages(&all_history, info);

    while (wait(NULL) > 0) {
    }

    close_pipes(info, true);
    print_history(&all_history);
}


void do_work(local_id process_count, balance_t *balances) {
    Info *info = malloc(sizeof(Info));
    info->s_process_count = process_count;

    FILE *events_file_ptr = fopen(events_log, "a");
    pipes_file_ptr = fopen(pipes_log, "a");

    init_topology(info);
    open_pipes(info);
    fork_processes(process_count, info, balances, events_file_ptr);
    parent_work(info);

    fclose(events_file_ptr);
    fclose(pipes_file_ptr);
}

