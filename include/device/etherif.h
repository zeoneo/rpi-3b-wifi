#ifndef _ETHER_INTERFACE_H_
#define _ETHER_INTERFACE_H_


#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>
#include<device/netif.h>

enum
{
	MaxEther	= 4,
	Ntypes		= 8,
};

#define NISAOPT		8

typedef struct {
	char	*type;
	uint32_t	port;
	int	irq;
	uint32_t	dma;
	uint32_t	mem;
	uint32_t	size;
	uint32_t	freq;

	int	nopt;
	char	*opt[NISAOPT];
} ISAConf;

typedef struct Ether Ether;
struct Ether {
	ISAConf isa_conf;			/* hardware info */

	int	ctlrno;
	int	minmtu;
	int 	maxmtu;

	Netif netif;

	void	(*attach)(Ether*);	/* filled in by reset routine */
	void	(*detach)(Ether*);
	void	(*transmit)(Ether*);
	void	(*interrupt)(void*);
	long	(*ifstat)(Ether*, void*, long, uint32_t);
	long 	(*ctl)(Ether*, void*, long); /* custom ctl messages */
	void	(*power)(Ether*, int);	/* power on/off */
	void	(*shutdown)(Ether*);	/* shutdown hardware before reboot */

	void*	ctlr;
	uint8_t	ea[Eaddrlen];
	void*	address;
	int	irq;

	Queue*	oq;
};

extern Block* etheriq(Ether*, Block*, int);
extern void addethercard(char*, int(*)(Ether*));
extern uint32_t ethercrc(uint8_t*, int);
extern int parseether(uint8_t*, char*);

#define NEXT(x, l)	(((x)+1)%(l))
#define PREV(x, l)	(((x) == 0) ? (l)-1: (x)-1)

#ifdef __cplusplus
}
#endif

#endif
