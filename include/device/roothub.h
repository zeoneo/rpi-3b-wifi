#ifndef ROOT_HUB_H
#define ROOT_HUB_H

#include <device/hcd.h>
#include <device/usbd.h>
#include <device/usb-mem.h>
#include <stdint.h>
#include <kernel/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    Result HcdProcessRootHubMessage(struct UsbDevice *device,
                                    struct UsbPipeAddress pipe, void *buffer, uint32_t bufferLength,
                                    struct UsbDeviceRequest *request);

#ifdef __cplusplus
}
#endif

#endif