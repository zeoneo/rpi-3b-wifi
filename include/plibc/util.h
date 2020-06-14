#ifndef _LIB_UTIL_
#define _LIB_UTIL_

#ifdef __cplusplus
extern "C" {
#endif

#include<stdint.h>


inline unsigned short bswap16(unsigned short v) {
    return ((v & 0xff) << 8) | (v >> 8);
}

inline unsigned int bswap32(const void* v) {
    uint8_t* b = (uint8_t*) v;
    uint8_t b1 = *b;
    uint8_t b2 = *(b + 1);
    uint8_t b3 = *(b + 2);
    uint8_t b4 = *(b + 3);

    return (b1 << 24 | b2 << 16 | b3 << 8 | b4);
}

#define BE(value)	((((value) & 0xFF00) >> 8) | (((value) & 0x00FF) << 8))


#define le2be16 bswap16
#define le2be32 bswap32

#define be2le16 bswap16
#define be2le32 bswap32

void hexdump(void* pStart, uint32_t nBytes);

#ifdef __cplusplus
}
#endif

#endif
