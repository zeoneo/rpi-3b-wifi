#include <kernel/queue.h>
#include <plibc/string.h>

queue* new_queue(uint32_t data_size) {
    return new_list(data_size);
}

void destroy_queue(queue* q) {
    destroy_list(q);
}

uint32_t queue_size(queue* q) {
    return list_size(q);
}

q_node* queue_head(queue* q) {
    return list_head(q);
}

q_node* queue_tail(queue* q) {
    return list_tail(q);
}

void enqueue(queue* q, void* data) {
    list_push_back(q, data);
}

void dequeue(queue* q, void* buffer) {
    if (queue_size(q) == 0) {
        return;
    }
    q_node* head = list_head(q);
    memcpy(buffer, head->data, q->data_size);
    list_delete_head(q);
}