#include <mem/kernel_alloc.h>
#include <net/net_queue.h>
#include <plibc/stdio.h>
net_queue_t* get_new_net_queue() {
    net_queue_t* q = kernel_allocate(sizeof(net_queue_t));
    q->head        = 0;
    q->tail        = 0;
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
    // assert (pEntry != 0);
    // assert (nLength > 0);
    // assert (nLength <= FRAME_BUFFER_SIZE);

    pEntry->nLength = nLength;

    // assert (pBuffer != 0);
    if (pBuffer == 0) {
        return;
    }
    memcpy(pEntry->Buffer, pBuffer, nLength);

    pEntry->pParam = pParam;

    // m_SpinLock.Acquire ();
    lock(&(nqueue->m_SpinLock));

    pEntry->prev = nqueue->tail;
    pEntry->next = 0;

    if (nqueue->head == 0) {
        nqueue->head = pEntry;
    } else {
        // assert ();
        // assert ();
        if (nqueue->tail != 0 && nqueue->tail->next == 0) {
            nqueue->tail->next = pEntry;
        }
    }
    nqueue->tail = pEntry;
    unlock(&(nqueue->m_SpinLock));
    // m_SpinLock.Release ();
}

// returns length (0 if queue is empty)
unsigned dequeue_nqueue(net_queue_t* nqueue, void* pBuffer, void** ppParam) {
    unsigned nResult = 0;
    // printf("dequeu 1\n");
    if (nqueue->head != 0) {
        lock(&(nqueue->m_SpinLock));
        volatile net_queue_entry_t* pEntry = nqueue->head;
        // assert(pEntry != 0);
        // printf("dequeu 2 %x \n",  pEntry->next);
        // printf("dequeu 2.1 length= %d \n",  pEntry->nLength);
        // printf("dequeu 2.2 \n");
        nqueue->head = pEntry->next;
        if (nqueue->head != 0) {
            // printf("dequeu 3 %x \n",  nqueue->head->prev);
            nqueue->head->prev = 0;
        } else {
            // printf("dequeu 4\n");
            // printf("dequeu 4 %x %x\n",  nqueue->tail, pEntry);
            if (nqueue->tail == pEntry) {
                nqueue->tail = 0;
            }
        }
        // printf("dequeu 5 %x \n",  nqueue->m_SpinLock);
        unlock(&(nqueue->m_SpinLock));
        // printf("dequeu 6 %x %d \n",  pEntry, pEntry->nLength);
        nResult = pEntry->nLength;
        // assert (nResult > 0);
        // assert (nResult <= FRAME_BUFFER_SIZE);
        // printf("deque 7 \n");
        if (nResult > 0 && nResult <= FRAME_BUFFER_SIZE) {
            // printf("deque 8 \n");
            memcpy(pBuffer, (const void*) pEntry->Buffer, nResult);
            // printf("deque 9 \n");
            if (ppParam != 0) {
                *ppParam = pEntry->pParam;
                            printf("deque 10 \n");

            }
            kernel_deallocate((net_queue_entry_t*)pEntry);
        }
        // printf("deque 10 \n");

    }
    // printf("deque 11 %d \n", nResult);
    return nResult;
}