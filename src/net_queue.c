#include <mem/kernel_alloc.h>
#include <net/net_queue.h>
#include <plibc/stdio.h>
net_queue_t* get_new_net_queue() {
    net_queue_t* q = kernel_allocate(sizeof(net_queue_t));
    q->head        = 0;
    q->tail        = 0;
    printf("new queue: q: 0x%x head:%x tail: %x lock: %x \n", q, q->head,  q->tail, &(q->m_SpinLock));
    return q;
}

bool is_empty_nqueue(net_queue_t* nqueue) {
    return nqueue->head == 0;
}

void flush_nqueue(net_queue_t* nqueue) {

    while (nqueue->head != 0) {
        lock(&(nqueue->m_SpinLock));
        volatile net_queue_entry_t* pEntry = nqueue->head;

        nqueue->head = pEntry->next;
        if (nqueue->head != 0) {
            nqueue->head->prev = 0;
        } else {
            // assert (m_pLast == pEntry);
            nqueue->tail = 0;
        }
        unlock(&(nqueue->m_SpinLock));
        kernel_deallocate((net_queue_entry_t*)pEntry);
    }
}

void enqueue_nqueue(net_queue_t* nqueue, const void* pBuffer, unsigned nLength, void* pParam) {
    net_queue_entry_t* pEntry = kernel_allocate(sizeof(net_queue_entry_t));

    if (pEntry == 0 || nLength == 0 || nLength > FRAME_BUFFER_SIZE) {
        return;
    }

    pEntry->prev = 0;
    pEntry->next = 0;
    pEntry->nLength = nLength;

    if (pBuffer == 0) {
        return;
    }
    memcpy(pEntry->Buffer, pBuffer, nLength);
    pEntry->pParam = pParam;

    lock(&(nqueue->m_SpinLock));

    if(nqueue->tail == 0) {
        nqueue->tail = pEntry;
        nqueue->head = nqueue->tail;
    } else {
        pEntry->prev = nqueue->tail;
        nqueue->tail->next = pEntry;
        // printf("Assining nqueue->next : %x \n", pEntry);
        nqueue->tail = pEntry;
    }
    unlock(&(nqueue->m_SpinLock));
}

// returns length (0 if queue is empty)
unsigned dequeue_nqueue(net_queue_t* nqueue, void* pBuffer, void** ppParam) {
    unsigned nResult = 0;
    if(is_empty_nqueue(nqueue)) {
        return nResult;
    }
    // printf("dequeu 1 nqueue->head: %x nqueue->head->next: %x \n", nqueue->head,  nqueue->head->next);

    volatile net_queue_entry_t* pEntry = nqueue->head;

    // printf("dequeu 2 %x \n",  pEntry->next);
    // printf("dequeu 2.1 length= %d \n",  pEntry->nLength);
    // printf("dequeu 2.2 nqueue->head: %x nqueue->tail: %x \n", nqueue->head, nqueue->tail);

    // Copying head into input buffer
    nResult = pEntry->nLength;
    memcpy(pBuffer, (const void*) pEntry->Buffer, nResult);
    if (ppParam != 0) {
        *ppParam = pEntry->pParam;
        // printf(" deque param copy \n");
    }

    lock(&(nqueue->m_SpinLock));
    if(pEntry->next != 0) {
        pEntry = pEntry->next;
        kernel_deallocate((net_queue_entry_t*)(nqueue->head));
        pEntry->prev = 0;
        nqueue->head = pEntry;
    } else {
        kernel_deallocate((net_queue_entry_t*)(nqueue->head));
        nqueue->head = 0;
        nqueue->tail = 0;
    }
    unlock(&(nqueue->m_SpinLock));

    return nResult;
}