#ifndef USB_H
#define USB_H

#include <kernel/types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
        \brief The maximum number of children a device could have, by implication, this is
        the maximum number of ports a hub supports.

        This is theoretically 255, as 8 bits are used to transfer the port count in
        a hub descriptor. Practically, no hub has more than 10, so we instead allow
        that many. Increasing this number will waste space, but will not have
        adverse consequences up to 255. Decreasing this number will save a little
        space in the HubDevice structure, at the risk of removing support for an
        otherwise valid hub.
*/
#define MaxChildrenPerDevice 10
/**
        \brief The maximum number of interfaces a device configuration could have.

        This is theoretically 255 as one byte is used to transfer the interface
        count in a configuration descriptor. In practice this is unlikely, so we
        allow an arbitrary 8. Increasing this number wastes (a lot) of space in
        every device structure, but should not have other consequences up to 255.
        Decreasing this number reduces the overheads of the UsbDevice structure, at
        the cost of possibly rejecting support for an otherwise supportable device.
*/
#define MaxInterfacesPerDevice 8
/**
        \brief The maximum number of endpoints a device could have (per interface).

        This is theoretically 16, as four bits are used to transfer the endpoint
        number in certain device requests. This is possible in practice, so we
        allow that many. Decreasing this number reduces the space in each device
        structure considerably, while possible removing support for otherwise valid
        devices. This number should not be greater than 16.
*/
#define MaxEndpointsPerDevice 16

/**
\brief Status of a USB transfer.

Stores the status of the last transfer a USB device did.
*/

/**
\brief Status of a USB device.

Stores the status of a USB device. Statuses as defined in 9.1 of the USB2.0
manual.
*/
enum UsbDeviceStatus {
    Attached   = 0,
    Powered    = 1,
    Default    = 2,
    Addressed  = 3,
    Configured = 4,
};

enum UsbTransferError {
    NoError         = 0,
    Stall           = 1 << 1,
    BufferError     = 1 << 2,
    Babble          = 1 << 3,
    NoAcknowledge   = 1 << 4,
    CrcError        = 1 << 5,
    BitError        = 1 << 6,
    ConnectionError = 1 << 7,
    AhbError        = 1 << 8,
    NotYetError     = 1 << 9,
    Processing      = 1 << 31
};

/**
\brief Start of a device specific data field.

The first two words of driver data in a UsbDevice. The  DeviceDriver field
is a code which uniquely identifies the driver that set the driver data
field (i.e. the lowest driver in the stack above the USB driver). The
DataSize is the size in bytes of the device specific data field.
*/
struct UsbDriverDataHeader {
    uint32_t DeviceDriver;
    uint32_t DataSize;
};

enum DescriptorType {
    Device                  = 1,
    Configuration           = 2,
    String                  = 3,
    Interface               = 4,
    Endpoint                = 5,
    DeviceQualifier         = 6,
    OtherSpeedConfiguration = 7,
    InterfacePower          = 8,
    Hid                     = 33,
    HidReport               = 34,
    HidPhysical             = 35,
    Hub                     = 41,
};

struct UsbDescriptorHeader {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
} __attribute__((__packed__));

/**
\brief The device descriptor information.

The device descriptor sturcture defined in the USB 2.0 manual in 9.6.1.
*/
enum DeviceClass {
    DeviceClassInInterface    = 0x00,
    DeviceClassCommunications = 0x2,
    DeviceClassHub            = 0x9,
    DeviceClassDiagnostic     = 0xdc,
    DeviceClassMiscellaneous  = 0xef,
    DeviceClassVendorSpecific = 0xff,
};

struct UsbDeviceDescriptor {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
    uint16_t UsbVersion;                    // (in BCD 0x210 = USB2.10) +0x2
    enum DeviceClass Class : 8;             // +0x4
    uint8_t SubClass;                       // +0x5
    uint8_t Protocol;                       // +0x6
    uint8_t MaxPacketSize0;                 // +0x7
    uint16_t VendorId;                      // +0x8
    uint16_t ProductId;                     // +0xa
    uint16_t Version;                       // +0xc
    uint8_t Manufacturer;                   // +0xe
    uint8_t Product;                        // +0xf
    uint8_t SerialNumber;                   // +0x10
    uint8_t ConfigurationCount;             // +0x11
} __attribute__((__packed__));

struct UsbConfigurationDescriptor {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
    uint16_t TotalLength;                   // +0x2
    uint8_t InterfaceCount;                 // +0x4
    uint8_t ConfigurationValue;             // +0x5
    uint8_t StringIndex;                    // +0x6
    struct {
        unsigned _reserved0_4 : 5;            // @0
        bool RemoteWakeup : 1;                // @5
        bool SelfPowered : 1;                 // @6
        unsigned _reserved7 : 1;              // @7
    } __attribute__((__packed__)) Attributes; // +0x7
    uint8_t MaximumPower;                     // +0x8
} __attribute__((__packed__));

/**
\brief The interface descriptor information.

The interface descriptor structure defined in the USB2.0 manual section
9.6.5.
*/
struct UsbInterfaceDescriptor {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
    uint8_t Number;                         // +0x2
    uint8_t AlternateSetting;               // +0x3
    uint8_t EndpointCount;                  // +0x4
    enum InterfaceClass {
        InterfaceClassReserved            = 0x0,
        InterfaceClassAudio               = 0x1,
        InterfaceClassCommunications      = 0x2,
        InterfaceClassHid                 = 0x3,
        InterfaceClassPhysical            = 0x5,
        InterfaceClassImage               = 0x6,
        InterfaceClassPrinter             = 0x7,
        InterfaceClassMassStorage         = 0x8,
        InterfaceClassHub                 = 0x9,
        InterfaceClassCdcData             = 0xa,
        InterfaceClassSmartCard           = 0xb,
        InterfaceClassContentSecurity     = 0xd,
        InterfaceClassVideo               = 0xe,
        InterfaceClassPersonalHealthcare  = 0xf,
        InterfaceClassAudioVideo          = 0x10,
        InterfaceClassDiagnosticDevice    = 0xdc,
        InterfaceClassWirelessController  = 0xe0,
        InterfaceClassMiscellaneous       = 0xef,
        InterfaceClassApplicationSpecific = 0xfe,
        InterfaceClassVendorSpecific      = 0xff,
    } Class : 8; // +x05
    uint8_t SubClass;
    uint8_t Protocol;
    uint8_t StringIndex;
} __attribute__((__packed__));

/**
\brief The endpoint descriptor information.

The endpoint descriptor structure defined in the USB2.0 manual section
9.6.6.
*/
struct UsbEndpointDescriptor {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
    struct {
        unsigned Number : 4;                       // @0
        unsigned _reserved4_6 : 3;                 // @4
        UsbDirection Direction : 1;                // @7
    } __attribute__((__packed__)) EndpointAddress; // +0x2
    struct {
        UsbTransfer Type : 2; // @0
        enum {
            NoSynchronisation = 0,
            Asynchronous      = 1,
            Adaptive          = 2,
            Synchrouns        = 3,
        } Synchronisation : 2; // @2
        enum {
            Data                = 0,
            Feeback             = 1,
            ImplicitFeebackData = 2,
        } Usage : 2;                          // @4
        unsigned _reserved6_7 : 2;            // @6
    } __attribute__((__packed__)) Attributes; // +0x3
    struct {
        unsigned MaxSize : 11; // @0
        enum {
            None   = 0,
            Extra1 = 1,
            Extra2 = 2,
        } Transactions : 2;               // @11
        unsigned _reserved13_15 : 3;      // @13
    } __attribute__((__packed__)) Packet; // +0x4
    uint8_t Interval;                     // +0x6
} __attribute__((__packed__));

/**
\brief The string descriptor information.

The string descriptor structure defined in the USB2.0 manual section
9.6.7.
*/
struct UsbStringDescriptor {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
    uint16_t Data[];                        // +0x2 amount varies
} __attribute__((__packed__));

/**
\brief Structure to store the details of a USB device that has been
detectd.

Stores the details about a connected USB device. This is not directly part
of the USB standard, and is instead a mechanism used to control the device
tree.
*/
struct UsbDevice {
    uint32_t Number;

    UsbSpeed Speed;
    enum UsbDeviceStatus Status;
    volatile uint8_t ConfigurationIndex;
    uint8_t PortNumber; // todo uint8_t PortNumber;
    volatile enum UsbTransferError Error __attribute__((aligned(4)));

    // Generic device handlers
    /** Handler for detaching the device. The device driver should not issue further requests to the device. */
    void (*DeviceDetached)(struct UsbDevice* device) __attribute__((aligned(4)));
    /** Handler for deallocation of the device. All memory in use by the device driver should be deallocated. */
    void (*DeviceDeallocate)(struct UsbDevice* device);
    /** Handler for checking for changes to the USB device tree. Only hubs need handle with this. */
    void (*DeviceCheckForChange)(struct UsbDevice* device);
    /** Handler for removing a child device from this device. Only hubs need handle with this. */
    void (*DeviceChildDetached)(struct UsbDevice* device, struct UsbDevice* child);
    /** Handler for reseting a child device of this device. Only hubs need handle with this. */
    Result (*DeviceChildReset)(struct UsbDevice* device, struct UsbDevice* child);
    /** Handler for reseting a child device of this device. Only hubs need handle with this. */
    Result (*DeviceCheckConnection)(struct UsbDevice* device, struct UsbDevice* child);

    volatile struct UsbDeviceDescriptor Descriptor __attribute__((aligned(4)));
    volatile struct UsbConfigurationDescriptor Configuration __attribute__((aligned(4)));
    volatile struct UsbInterfaceDescriptor Interfaces[MaxInterfacesPerDevice] __attribute__((aligned(4)));
    volatile struct UsbEndpointDescriptor Endpoints[MaxInterfacesPerDevice][MaxEndpointsPerDevice]
        __attribute__((aligned(4)));
    struct UsbDevice* Parent __attribute__((aligned(4)));
    volatile void* FullConfiguration;
    volatile struct UsbDriverDataHeader* DriverData;
    volatile uint32_t LastTransfer;
};

struct __attribute__((__packed__)) UsbDeviceRequest {
    uint8_t Type; // +0x0
    enum UsbDeviceRequestRequest {
        // USB requests
        GetStatus        = 0,
        ClearFeature     = 1,
        SetFeature       = 3,
        SetAddress       = 5,
        GetDescriptor    = 6,
        SetDescriptor    = 7,
        GetConfiguration = 8,
        SetConfiguration = 9,
        GetInterface     = 10,
        SetInterface     = 11,
        SynchFrame       = 12,
        // HID requests
        GetReport   = 1,
        GetIdle     = 2,
        GetProtocol = 3,
        SetReport   = 9,
        SetIdle     = 10,
        SetProtocol = 11,
    } Request : 8;   // +0x1
    uint16_t Value;  // +0x2
    uint16_t Index;  // +0x4
    uint16_t Length; // +0x6
};

struct __attribute__((__packed__)) UsbPipeAddress {
    UsbPacketSize MaxSize : 2;    // @0
    UsbSpeed Speed : 2;           // @2
    unsigned EndPoint : 4;        // @4
    unsigned Device : 8;          // @8
    UsbTransfer Type : 2;         // @16
    UsbDirection Direction : 1;   // @18
    unsigned _reserved19_31 : 13; // @19
};

Result UsbInitialise();
void UsbShowTree(struct UsbDevice* root, const int level, const char tee);
Result UsbAttachRootHub();
Result UsbAllocateDevice(struct UsbDevice** device);
Result UsbAttachDevice(struct UsbDevice* device);
void UsbDeallocateDevice(struct UsbDevice* device);
Result UsbReadDeviceDescriptor(struct UsbDevice* device);
Result UsbGetDescriptor(struct UsbDevice* device, enum DescriptorType type, uint8_t index, uint16_t langId,
                        void* buffer, uint32_t length, uint32_t minimumLength, uint8_t recipient);
const char* UsbGetDescription(struct UsbDevice* device);
Result UsbSetConfiguration(struct UsbDevice* device, uint8_t configuration);
Result UsbSetAddress(struct UsbDevice* device, uint8_t address);
Result UsbSetInterface(struct UsbDevice* device, uint8_t interface);
Result UsbReadString(struct UsbDevice* device, uint8_t stringIndex, char* buffer, uint32_t length);
Result UsbConfigure(struct UsbDevice* device, uint8_t configuration);
Result UsbSetConfiguration(struct UsbDevice* device, uint8_t configuration);
Result UsbControlMessage(struct UsbDevice* device, struct UsbPipeAddress pipe, void* buffer, uint32_t bufferLength,
                         struct UsbDeviceRequest* request, uint32_t timeout);
Result UsbGetString(struct UsbDevice* device, uint8_t stringIndex, uint16_t langId, void* buffer, uint32_t length);
void UsbCheckForChange();
#ifdef __cplusplus
}
#endif

#endif