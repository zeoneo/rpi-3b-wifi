#ifndef ROOT_HUB_H
#define ROOT_HUB_H

#include <device/hcd.h>
#include <device/usb-mem.h>
#include <device/usbd.h>
#include <kernel/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

Result HcdProcessRootHubMessage(struct UsbDevice* device, struct UsbPipeAddress pipe, void* buffer,
                                uint32_t bufferLength, struct UsbDeviceRequest* request);

#ifdef __cplusplus
}
#endif

#endif