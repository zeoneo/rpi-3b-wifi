
#ifndef _QUEUE_H_
#define _QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/list.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*listIterator)(void*);

typedef struct l_node q_node;
typedef list queue;

queue* new_queue(uint32_t data_size);
void destroy_queue(queue* q);
uint32_t queue_size(queue* q);

q_node* queue_head(queue* q);
q_node* queue_tail(queue* q);

void enqueue(list* q, void* data);
void dequeue(list* q, void* buffer);

#ifdef __cplusplus
}
#endif

#endif // LIST_H_