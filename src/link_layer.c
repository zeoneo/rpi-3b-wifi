#include <net/link_layer.h>
#include <net/net_queue.h>
#include <mem/kernel_alloc.h>
#include <plibc/util.h>
#include <plibc/stdio.h>


static bcm4343_net_device* attached_net_device = 0;
static uint16_t raw_protocol_type;
static net_queue_t* rx_net_queue;
static net_queue_t* tx_net_queue;
static uint8_t broadcast_addr[MAC_ADDRESS_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct TRawPrivateData {
    unsigned char MACSender[MAC_ADDRESS_SIZE];
};

bool initialize_link_layer(bcm4343_net_device* net_device) {
    if (attached_net_device == 0) {
        attached_net_device = net_device;
        rx_net_queue        = get_new_net_queue();
        tx_net_queue        = get_new_net_queue();
        return true;
    }
    return false;
}

bool send_raw_l2_packet(const void* pFrame, unsigned nLength) {
    // assert (pFrame != 0);
    // assert (nLength > 0);
    // assert (m_pNetDevLayer != 0);
    if (pFrame != 0 && nLength > 0 && attached_net_device != 0) {
        return send_frame(attached_net_device, pFrame, nLength);
    }
    return false;
}

// nProtocolType is in host byte order
bool enable_receive_raw_l2(uint16_t nProtocolType) {
    if (raw_protocol_type != 0) {
        return false;
    }

    if (nProtocolType != 0) {
        raw_protocol_type = le2be16(nProtocolType);
        return true;
    }
    return false;
}

bool receive_raw_l2_packet(void* pBuffer, unsigned* pResultLength, uint8_t* sender_mac) {
    void* pParam;
    // assert (pBuffer != 0);
    // assert (pResultLength != 0);

    if (pBuffer == 0 || pResultLength == 0) {
        return false;
    }

    *pResultLength = dequeue_nqueue(rx_net_queue, pBuffer, &pParam);
    if (*pResultLength == 0) {
        return false;
    }

    struct TRawPrivateData* pData = (struct TRawPrivateData*) pParam;

    if (sender_mac != 0 && pData != 0) {
        memcpy(sender_mac, pData->MACSender, MAC_ADDRESS_SIZE);
    }

    return true;
}


void process_l2_layer(void) {

    // assert (m_pNetDevLayer != 0);
    const uint8_t* pOwnMACAddress = get_mac_address(attached_net_device);

    if (pOwnMACAddress == 0) {
        return;
    }

    uint8_t Buffer[FRAME_BUFFER_SIZE];
    unsigned nLength;
    while (receive_frame(attached_net_device, Buffer, (uint32_t *)&nLength)) {
        // assert (nLength <= FRAME_BUFFER_SIZE);
        if (nLength > FRAME_BUFFER_SIZE) {
            printf(" Warning nLength is greater thatn frame size \n");
            continue;
        }

        if (nLength <= sizeof(struct TEthernetHeader)) {
            continue;
        }
        struct TEthernetHeader* pHeader = (struct TEthernetHeader*) Buffer;

        uint8_t MACAddressReceiver[MAC_ADDRESS_SIZE] = {0};
        memcpy(MACAddressReceiver, pHeader->MACReceiver, MAC_ADDRESS_SIZE);

        if (memcmp(MACAddressReceiver, pOwnMACAddress, MAC_ADDRESS_SIZE) != 0 &&
            memcmp(broadcast_addr, MACAddressReceiver, MAC_ADDRESS_SIZE) != 0) {
            continue;
        }

        nLength -= sizeof(struct TEthernetHeader);
        // assert(nLength > 0);

        switch (pHeader->nProtocolType) {
            // case BE (ETH_PROT_IP):
            //     enqueue_nqueue(rx_net_queue, Buffer + sizeof (struct TEthernetHeader), nLength);
            // 	break;

            // case BE (ETH_PROT_ARP):
            // 	m_ARPRxQueue.Enqueue (Buffer+sizeof (TEthernetHeader), nLength);
            // 	break;

        default:
            if (pHeader->nProtocolType == raw_protocol_type) {
                struct TRawPrivateData* pParam = kernel_allocate(sizeof(struct TRawPrivateData));
                // assert(pParam != 0);
                if (pParam != 0) {
                    memcpy(pParam->MACSender, pHeader->MACSender, MAC_ADDRESS_SIZE);
                    enqueue_nqueue(rx_net_queue, Buffer + sizeof(struct TEthernetHeader), nLength, pParam);
                }
            }
            break;
        }
    }

    // assert (m_pARPHandler != 0);
    // m_pARPHandler->Process ();
}