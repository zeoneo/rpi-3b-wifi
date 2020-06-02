#ifndef _DATA_STRUCT_H_
#define _DATA_STRUCT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint64_t path;
    uint32_t vers;
    uint8_t type;
} Qid;

// typedef struct {
//     uint32_t key;
//     uint32_t sr;
//     uint32_t pc;
//     uint16_t isilock;
//     long lockcycles;
// } Lock;

// typedef struct {
//     Lock lock;
//     long ref;
// } Ref;

typedef struct Chan Chan;
typedef struct Path Path;
typedef struct Block Block;
typedef struct Queue Queue;

struct Chan {
    // Ref ref;    /* the Lock in this Ref is also Chan's lock */
    Chan* next; /* allocation */
    Chan* link;
    int64_t offset;    /* in fd */
    int64_t devoffset; /* in underlying device; see read */
    uint16_t type;
    uint32_t dev;
    uint16_t mode; /* read/write */
    uint16_t flag;
    Qid qid;
    int fid;         /* for devmnt */
    uint32_t iounit; /* chunk size for i/o; 0==default */
    // Mhead*	umh;			/* mount point that derived Chan; used in unionread */
    Chan* umc; /* channel in union; held for union read */
    // QLock	umqlock;		/* serialize unionreads */
    int uri;          /* union read index */
    int dri;          /* devdirread index */
    uint8_t* dirrock; /* directory entry rock for translations */
    int nrock;
    int mrock;
    // QLock	rockqlock;
    int ismtpt;
    // Mntcache*mcp;			/* Mount cache pointer */
    // Mnt*	mux;			/* Mnt for clients using me for messages */
    union {
        void* aux;
        Qid pgrpid;   /* for #p/notepg */
        uint32_t mid; /* for ns in devproc */
    };
    Chan* mchan; /* channel to mounted server */
    Qid mqid;    /* qid of root of mount point */
    Path* path;
};

struct Path {
    // Ref ref;
    char* s;
    Chan** mtpt; /* mtpt history */
    int len;     /* strlen(s) */
    int alen;    /* allocated length of s */
    int mlen;    /* number of path elements */
    int malen;   /* allocated length of mtpt */
};

typedef struct {
    Chan* clone;
    int nqid;
    Qid qid[1];
} Walkqid;

struct Queue {
    // Lock lock;

    Block* first; /* buffer */
    Block* last;

    int len;    /* bytes allocated to queue */
    int dlen;   /* data bytes in queue */
    int limit;  /* max bytes in queue */
    int inilim; /* initial limit */
    uint32_t nelem;
    int state;
    int noblock; /* true if writes return immediately when q full */
    int eof;     /* number of eofs read by user */

    void (*kick)(void*);           /* restart output */
    void (*bypass)(void*, Block*); /* bypass queue altogether */
    void* arg;                     /* argument to kick */

    // QLock	rlock;		/* mutex for reading processes */
    // Rendez	rr;		/* process waiting to read */
    // QLock	wlock;		/* mutex for writing processes */
    // Rendez	wr;		/* process waiting to write */

    char err[256];
};

struct Block {
    long ref;
    Block* next;
    Block* list;
    uint8_t* rp;  /* first unconsumed byte */
    uint8_t* wp;  /* first empty byte */
    uint8_t* lim; /* 1 past the end of the buffer */
    uint8_t* buf; /* start of the buffer */
    void (*free)(Block*);
    uint16_t flag;
    uint16_t checksum; /* IP checksum of complete packet (minus media header) */
    uint32_t magic;
};

#ifdef __cplusplus
}
#endif

#endif
