#include <device/etherif.h>
#include <mem/kernel_alloc.h>
#include <plibc/stdio.h>
#include <plibc/string.h>
#include <plibc/util.h>
#include <stdint.h>

#define le2be16 bswap16
#define le2be32 bswap32

#define be2le16 bswap16
#define be2le32 bswap32

Block* allocb(uint32_t size) // TODO: use single new
{
    uint32_t maxhdrsize = 64;

    Block* b = kernel_allocate(sizeof(Block));
    if (b == 0) {
        printf("Error allocating block with intial buffer size: %d \n", size);
        return 0;
    }

    size += maxhdrsize;

    b->buf = kernel_allocate(size);
    // assert (b->buf != 0);

    if (b->buf == 0) {
        printf("Error allocating buffer of size: %d \n", size);
        return 0;
    }

    b->next = 0;
    b->lim  = b->buf + size;
    b->wp   = b->buf + maxhdrsize;
    b->rp   = b->wp;
    // printf("b:%x wp: %x rp: %x \n", b, b->wp, b->rp);
    return b;
}

void freeb(Block* b) {
    kernel_deallocate(b->buf);
    kernel_deallocate(b);
}

Block* copyblock(Block* b, uint32_t size) {
    // assert (0);			// TODO: not tested

    Block* b2 = allocb(size);
    if (b2 == 0) {
        printf("Error in mem allocate. \n");
        return 0;
    }

    uint32_t len = b->wp - b->buf;
    if (len > 0) {
        memcpy(b2->buf, b->buf, len);

        b2->wp += len;
    }

    return b2;
}

Block* padblock(Block* b, int size) {
    // assert (size > 0);
    if (size <= 0 || b == 0 || (b->rp - b->buf < size)) {
        printf("size error. \n");
        return NULL;
    }
    // assert (b != 0);
    // assert (b->rp - b->buf >= size);
    b->rp -= size;

    return b;
}

unsigned qlen(Queue* q) {
    return q->nelem;
}

Block* qget(Queue* q) {
    Block* b = 0;

    // assert (q != 0);
    if (q == 0) {
        printf("Queue is zero \n");
        return 0;
    }
    if (q->first != 0) {
        b = q->first;

        q->first = b->next;
        if (q->first == 0) {
            // assert (q->last == b);
            if (q->last != b) {
                printf("Queue last is not equal to b \n");
                return 0;
            }
            q->last = 0;
        }
        if (q->nelem <= 0) {
            printf("Queue is zero or less ele \n");
            return 0;
        }
        // assert (q->nelem > 0);
        q->nelem--;
    }

    return b;
}

void qpass(Queue* q, Block* b) {
    // assert (b != 0);
    b->next = 0;

    // assert (q != 0);
    if (q->first == 0) {
        q->first = b;
    } else {
        // assert (q->last != 0);
        // assert (q->last->next == 0);
        q->last->next = b;
    }

    q->last = b;

    q->nelem++;
}

void qwrite(Queue* q, uint32_t x, uint32_t y) {
    printf("TODO qwrite called: %x %x %x \n", q, x, y);
}

uint16_t nhgets(const void* p) {
    return be2le16(*(uint16_t*) p);
}

uint32_t nhgetl(const void* p) {
    return be2le32(p);
}

int parseether(uint8_t* addr, const char* str) {
for (unsigned i = 1; i <= 6; i++)
	{
		char buf[3], *p = buf;

		if (*str == '\0')
		{
			return -1;
		}
		*p++ = *str++;

		if (*str == '\0')
		{
			return -1;
		}
		*p++ = *str++;

		if (i < 6 && *str == ':')
		{
			str++;
		}
		*p = '\0';

		char *end = 0;
		*addr++ = (unsigned char) strtoul (buf, &end, 16); //strtoul ??
		if (end != 0 && *end != '\0')
		{
			return -1;
		}
	}

	return *str == '\0' ? 0 : -1;
}
