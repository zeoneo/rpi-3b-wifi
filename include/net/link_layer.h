#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <device/bcm4343.h>

#define FRAME_BUFFER_SIZE 1600
#define MAC_ADDRESS_SIZE	6

struct TEthernetHeader {
    uint8_t MACReceiver[MAC_ADDRESS_SIZE];
    uint8_t MACSender[MAC_ADDRESS_SIZE];
    uint16_t nProtocolType;
#define ETH_PROT_IP  0x800
#define ETH_PROT_ARP 0x806
} __attribute__((packed));


bool initialize_link_layer(bcm4343_net_device *net_device);

// pBuffer must have size FRAME_BUFFER_SIZE
bool receive_raw_l2_packet(void *pBuffer, unsigned *pResultLength, uint8_t *sender_mac);

bool send_raw_l2_packet(const void *pFrame, unsigned nLength);
// nProtocolType is in host byte order
bool enable_receive_raw_l2(uint16_t nProtocolType);
void process_l2_layer(void);
void process_l2_layer_loop(void);
#ifdef __cplusplus
}
#endif

#endif
