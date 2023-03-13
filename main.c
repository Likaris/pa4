#include "work_manager.h"

//balance_t *get_balances(int count, char *argv[]) {
//    balance_t *balances = malloc(count * sizeof(balance_t));
//    for (int i = 0; i < count; i++) {
//        balances[i] = atoi(argv[i + 3]);
//    }
//
//    return balances;
//}

int main(int argc, char *argv[]) {
    local_id process_count = atoi(argv[2]);
    //balance_t *balances = get_balances(process_count, argv);
    do_work(process_count + 1);
    return 0;
}

