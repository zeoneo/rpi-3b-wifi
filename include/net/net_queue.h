
#ifndef _NET_QUEUE_H_
#define _NET_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/queue.h>
#include <kernel/lock.h>
#include <net/link_layer.h>
#include <stdbool.h>

typedef struct net_queue_entry net_queue_entry_t;
typedef struct net_queue net_queue_t;

struct net_queue_entry {
    volatile net_queue_entry_t* prev;
    volatile net_queue_entry_t* next;
    unsigned nLength;
    unsigned char Buffer[FRAME_BUFFER_SIZE];
    void* pParam;
};

struct net_queue {
	volatile net_queue_entry_t *head;
	volatile net_queue_entry_t *tail;
	Lock  m_SpinLock;
};


net_queue_t *get_new_net_queue();

bool is_empty_nqueue(net_queue_t *nqueue);
void flush_nqueue (net_queue_t *nqueue);
void enqueue_nqueue(net_queue_t *nqueue, const void *pBuffer, unsigned nLength, void *pParam);
// returns length (0 if queue is empty)
unsigned dequeue_nqueue(net_queue_t *nqueue, void *pBuffer, void **ppParam);

#ifdef __cplusplus
}
#endif

#endif // _NET_QUEUE_H_