#ifndef _PLAN9_NETIF_H_
#define _PLAN9_NETIF_H_


#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>
#include<device/data_struct.h>


typedef struct Etherpkt	Etherpkt;
typedef struct Netaddr	Netaddr;
typedef struct Netfile	Netfile;
typedef struct Netif	Netif;

enum
{
	Nmaxaddr=	64,
	Nmhash=		31,

	Ncloneqid=	1,
	Naddrqid,
	N2ndqid,
	N3rdqid,
	Ndataqid,
	Nctlqid,
	Nstatqid,
	Ntypeqid,
	Nifstatqid,
	Nmtuqid,
};

/*
 *  Macros to manage Qid's used for multiplexed devices
 */
#define NETTYPE(x)	(((uint32_t)x)&0x1f)
#define NETID(x)	((((uint32_t)x))>>5)
#define NETQID(i,t)	((((uint32_t)i)<<5)|(t))

#define	KNAMELEN		28

/*
 *  one per multiplexed connection
 */
struct Netfile
{
	// QLock;

	int	inuse;
	uint32_t	mode;
	char	owner[KNAMELEN];

	int	type;			/* multiplexor type */
	int	prom;			/* promiscuous mode */
	int	scan;			/* base station scanning interval */
	int	bridge;			/* bridge mode */
	int	headersonly;		/* headers only - no data */
	uint8_t	maddr[8];		/* bitmask of multicast addresses requested */
	int	nmaddr;			/* number of multicast addresses */

	Queue	*in;			/* input buffer */
};

/*
 *  a network address
 */
struct Netaddr
{
	Netaddr	*next;		/* allocation chain */
	Netaddr	*hnext;
	uint8_t	addr[Nmaxaddr];
	int	ref;
};

/*
 *  a network interface
 */
struct Netif
{
	// QLock;

	/* multiplexing */
	char	name[KNAMELEN];		/* for top level directory */
	int	nfile;			/* max number of Netfiles */
	Netfile	**f;

	/* about net */
	int	limit;			/* flow control */
	int	alen;			/* address length */
	int	mbps;			/* megabits per sec */
	int	link;			/* link status */
	int	minmtu;
	int 	maxmtu;
	int	mtu;
	uint8_t	addr[Nmaxaddr];
	uint8_t	bcast[Nmaxaddr];
	Netaddr	*maddr;			/* known multicast addresses */
	int	nmaddr;			/* number of known multicast addresses */
	Netaddr *mhash[Nmhash];		/* hash table of multicast addresses */
	int	prom;			/* number of promiscuous opens */
	int	scan;			/* number of base station scanners */
	int	all;			/* number of -1 multiplexors */

	/* statistics */
	int	misses;
	uint64_t	inpackets;
	uint64_t	outpackets;
	int	crcs;		/* input crc errors */
	int	oerrs;		/* output errors */
	int	frames;		/* framing errors */
	int	overflows;	/* packet overflows */
	int	buffs;		/* buffering errors */
	int	soverflows;	/* software overflow */

	/* routines for touching the hardware */
	void	*arg;
	void	(*promiscuous)(void*, int);
	void	(*multicast)(void*, uint8_t*, int);
	int	(*hwmtu)(void*, int);	/* get/set mtu */
	void	(*scanbs)(void*, uint32_t);	/* scan for base stations */
};

    void        netifinit(Netif*, char*, int, uint32_t);
    Walkqid*    netifwalk(Netif*, Chan*, Chan*, char **, int);
    Chan*       netifopen(Netif*, Chan*, int);
    void        netifclose(Netif*, Chan*);
    long        netifread(Netif*, Chan*, void*, long, uint32_t);
    Block*      netifbread(Netif*, Chan*, long, uint32_t);
    long        netifwrite(Netif*, Chan*, void*, long);
    int         netifwstat(Netif*, Chan*, uint8_t*, int);
    int         netifstat(Netif*, Chan*, uint8_t*, int);
    int         activemulti(Netif*, uint8_t*, int);

/*
 *  Ethernet specific
 */
enum
{
	Eaddrlen=	6,
	ETHERMINTU =	60,		/* minimum transmit size */
	ETHERMAXTU =	1514,		/* maximum transmit size */
	ETHERHDRSIZE =	14,		/* size of an ethernet header */

	/* ethernet packet types */
	ETARP		= 0x0806,
	ETIP4		= 0x0800,
	ETIP6		= 0x86DD,
};

struct Etherpkt
{
	uint8_t	d[Eaddrlen];
	uint8_t	s[Eaddrlen];
	uint8_t	type[2];
	uint8_t	data[1500];
};


#ifdef __cplusplus
}
#endif

#endif
