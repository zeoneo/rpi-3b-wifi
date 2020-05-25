#include<device/bcm4343.h>
#include<device/etherif.h>
#include<device/plan9_ether4330.h>
#include<plibc/stdio.h>
#include<plibc/string.h>
#include <mem/kernel_alloc.h>


#define MAX_BCM4343_DEV 1

static bcm4343_net_device bcm_dev[MAX_BCM4343_DEV];

bcm4343_net_device * allocate_bcm4343_device(char * firmware_path) {

    if(bcm_dev[0].is_used == 1) {
        return NULL;
    }
    //TODO: allocate net device here.
    bcm_dev[0].is_used = 1;
    bcm_dev[0].edev = kernel_allocate(sizeof(Ether));
    bcm_dev[0].net_device = kernel_allocate(sizeof(NetDevice));
    bcm_dev[0].net_device->in = new_queue(1600); // Frame size
    memcpy(&(bcm_dev[0].firmware_path[0]), firmware_path, strlen(firmware_path));
    return &bcm_dev[0];
}

void set_essid(bcm4343_net_device *net_device, char *essid) {
    char *str = format_str(net_device->essid, "essid %s", essid);
    *str = '\0';
}

void set_auth(bcm4343_net_device *net_device, TAuthMode auth_mode, char *auth_key) {
    net_device->auth_mode = auth_mode;
    if (auth_mode == AuthModeNone) {
        return;
    }

    size_t key_len = strlen (auth_key);
    char *str = format_str(net_device->auth_cmd, "auth %02X%02X",  auth_mode == AuthModeWPA ? 0xDD : 0x30, key_len);

	while (key_len-- > 0)
	{
        format_str(str, "%02X",  (uint8_t) *auth_key++);
	}
}

bool initialize (bcm4343_net_device *net_device) {

    etherbcmpnp(net_device->edev);
    net_device->edev->oq = kernel_allocate(sizeof(Queue));
	memset (net_device->edev->oq, 0, sizeof(*net_device->edev->oq));

    net_device->edev->attach(net_device->edev);
    memcpy(net_device->net_device->maddr, net_device->edev->ea, Eaddrlen);

	if (net_device->auth_mode != AuthModeNone)
	{
		net_device->edev->ctl(net_device->edev, (char *) net_device->auth_cmd, strlen(net_device->auth_cmd));
	}

	(net_device->edev->ctl) (net_device->edev, (char *) net_device->essid, strlen(net_device->essid));
	return true;
}

const uint8_t * get_mac_address (bcm4343_net_device *net_device) {
    return net_device->net_device->maddr;
}

bool send_frame(bcm4343_net_device *net_device, const void *frame_buffer, uint32_t length) {

	Block *block = allocb (length);

    if(block == NULL || block->wp != NULL || frame_buffer == NULL) {
        printf("NULL error \n");
        return false;
    }
	// assert (block != 0);
	// assert (block->wp != 0);
	// assert (pBuffer != 0);
	memcpy (block->wp, frame_buffer, length);
	block->wp += length;

	// assert (s_EtherDevice.oq != 0);
    if(net_device->edev->oq == NULL) {
        printf("Output queue is empty \n");
        return false;
    }
	qpass (net_device->edev->oq, block);

	// assert (s_EtherDevice.transmit != 0);
	net_device->edev->transmit(net_device->edev);
    return true;
}

static void hexdump (void *pStart, uint32_t nBytes)
{
	uint8_t *pOffset = (uint8_t *) pStart;

	printf( "Dumping 0x%X bytes starting at 0x%lX", nBytes, (unsigned long) (uint32_t *) pOffset);
	
	while (nBytes > 0)
	{
		printf("%04X: %02X %02X %02X %02X %02X %02X %02X %02X-%02X %02X %02X %02X %02X %02X %02X %02X",
				(unsigned) (uint32_t) pOffset & 0xFFFF,
				(unsigned) pOffset[0],  (unsigned) pOffset[1],  (unsigned) pOffset[2],  (unsigned) pOffset[3],
				(unsigned) pOffset[4],  (unsigned) pOffset[5],  (unsigned) pOffset[6],  (unsigned) pOffset[7],
				(unsigned) pOffset[8],  (unsigned) pOffset[9],  (unsigned) pOffset[10], (unsigned) pOffset[11],
				(unsigned) pOffset[12], (unsigned) pOffset[13], (unsigned) pOffset[14], (unsigned) pOffset[15]);

		pOffset += 16;

		if (nBytes >= 16)
		{
			nBytes -= 16;
		}
		else
		{
			nBytes = 0;
		}
	}
}

bool receive_frame (bcm4343_net_device *net_device, void *frame_buffer, uint32_t *length) {
	
    // How to enque and deque from queue? Implement the operations
    if(frame_buffer == NULL) {
        printf("Frame buffer is null: %d \n", *length);
        return false;
    }

    dequeue(net_device->net_device->in, frame_buffer);

	// unsigned nLength = m_RxQueue.Dequeue (pBuffer);
	// if (nLength == 0)
	// {
	// 	return FALSE;
	// }

	// assert (pResultLength != 0);
	// *pResultLength = nLength;

	hexdump (frame_buffer, net_device->net_device->in->data_size);
    *length = net_device->net_device->in->data_size;
	return true;
}

bool is_link_alive(bcm4343_net_device *net_device) {
    printf("net_device: %x \n", net_device);
    return true;
}

void dump_status(bcm4343_net_device *net_device) {
	
	// assert (s_EtherDevice.ifstat != 0);
	net_device->edev->ifstat(net_device->edev, 0, 0, 0);
	printf("----\n");
}

void etheriq (Ether *pEther, Block *pBlock, uint32_t nFlag)
{
    printf("nFlag: %d \n", nFlag);
    bcm4343_net_device * net_device = ( bcm4343_net_device *)pEther->net_dev;
    // assert (pBlock != 0);
    enqueue(net_device->net_device->in, pBlock->rp);
	freeb (pBlock);
}
