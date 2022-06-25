
#include <plibc/util.h>
#include <plibc/stdio.h>

void hexdump(void* pStart, uint32_t nBytes) {
    uint8_t* pOffset = (uint8_t*) pStart;

    printf("Dumping 0x%X bytes starting at 0x%lX", nBytes, (unsigned long) (uint32_t*) pOffset);

    while (nBytes > 0) {
        printf("%04X: %02X %02X %02X %02X %02X %02X %02X %02X-%02X %02X %02X %02X %02X %02X %02X %02X",
               (unsigned) (uint32_t) pOffset & 0xFFFF, (unsigned) pOffset[0], (unsigned) pOffset[1],
               (unsigned) pOffset[2], (unsigned) pOffset[3], (unsigned) pOffset[4], (unsigned) pOffset[5],
               (unsigned) pOffset[6], (unsigned) pOffset[7], (unsigned) pOffset[8], (unsigned) pOffset[9],
               (unsigned) pOffset[10], (unsigned) pOffset[11], (unsigned) pOffset[12], (unsigned) pOffset[13],
               (unsigned) pOffset[14], (unsigned) pOffset[15]);

        pOffset += 16;

        if (nBytes >= 16) {
            nBytes -= 16;
        } else {
            nBytes = 0;
        }
    }
}
