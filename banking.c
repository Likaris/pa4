#include "banking.h"
#include "main.h"

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    TransferOrder transferOrder = {src, dst, amount};
    Message msg = create_message(TRANSFER, sizeof(TransferOrder), &transferOrder);

    send(parent_data, src, &msg);

    Message m_received;
    receive(parent_data, dst, &m_received);
}


