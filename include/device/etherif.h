#ifndef _p9ether_h
#define _p9ether_h


#ifdef __cplusplus
extern "C" {
#endif

#include<stdint.h>
#include<device/netif.h>
#include<device/data_struct.h>

/*
 *  Ethernet specific
 */
enum
{
	Eaddrlen =	6,
	ETHERMINTU =	60,		/* minimum transmit size */
	ETHERMAXTU =	1514,		/* maximum transmit size */
	ETHERHDRSIZE =	14,		/* size of an ethernet header */

	/* ethernet packet types */
	ETARP		= 0x0806,
	ETIP4		= 0x0800,
	ETIP6		= 0x86DD,
};

// typedef struct Block
// {
// 	struct Block *next;
// 	uint8_t *buf;
// 	uint8_t *lim;
// 	uint8_t *wp;
// 	uint8_t *rp;
// #define BLEN(b)		((b)->wp - (b)->rp)
// }
// Block;

Block *allocb (uint32_t size);
void freeb (Block *b);
Block *copyblock (Block *b, uint32_t size);
Block *padblock (Block *b, int size);

// typedef struct
// {
// 	Block *first;
// 	Block *last;
// 	unsigned nelem;
// } Queue;

unsigned qlen (Queue *q);
Block *qget (Queue *q);
void qpass (Queue *q, Block *b);
void qwrite (Queue *q, uint32_t x, uint32_t y);

typedef struct Netfile Netfile;
// typedef struct
// {
// 	Queue *in;
// 	int inuse;
// 	uint32_t type;
// 	int scan;
// }
// Netfile;

typedef struct Ether
{
	void * net_dev;
	void *ctlr;
	void *arg;
	uint8_t ea[Eaddrlen];
	uint8_t addr[Eaddrlen];
	Queue *oq;
	int scan;

	unsigned nfile;
#define Ntypes		10
	Netfile *f[Ntypes];

	void (*attach) (struct Ether *edev);
	void (*transmit) (struct Ether *edev);
	long (*ifstat) (struct Ether *edev, void *buf, long size, uint32_t offset);
	long (*ctl) (struct Ether *edev, void *buf, long n);
	void (*scanbs) (void *arg, uint32_t secs);
	void (*shutdown) (struct Ether *edev);
}
Ether;

uint16_t nhgets (const void *p);
uint32_t nhgetl (const void *p);
int parseether (uint8_t *addr, const char *str);

// void etheriq (Ether *ether, Block *block, unsigned flag);

typedef int ether_pnp_t (Ether *edev);
void addethercard (const char *name, ether_pnp_t *ether_pnp);

#ifdef __cplusplus
}
#endif

#endif
