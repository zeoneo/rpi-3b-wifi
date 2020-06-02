#include <device/netif.h>

#define MAX_NET_DEV 8

#define FRAME_BUFFER_SIZE 1600

NetDevice net_devices[MAX_NET_DEV];

const char net_speed_table[NetDeviceSpeedUnknown] = {"10BASE-T Half-duplex",   "10BASE-T Full-duplex",
                                                     "100BASE-TX Half-duplex", "100BASE-TX Full-duplex",
                                                     "1000BASE-T Half-duplex", "1000BASE-T Full-duplex"};

static NetDevice* allocate_net_dev() {
    for (uint32_t i = 0; i < MAX_NET_DEV; i++) {
        if (net_devices[i].inuse == 0) {
            net_devices[i].device_number = i;
            return &net_devices[i];
        }
    }
    return NULL;
}

NetDevice* add_net_device() {
    NetDevice* net_device = allocate_net_dev();
    if (net_device == NULL) {
        return false;
    }
    net_device->inuse = 1;
    return net_device;
}

char* get_speed_str(NetDeviceSpeed net_speed) {
    if (net_speed >= NetDeviceSpeedUnknown) {
        return "Unknown";
    }
    return net_speed_table[net_speed]
}
