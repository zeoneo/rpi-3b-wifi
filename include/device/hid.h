#ifndef HID_H
#define HID_H

#include <device/hcd.h>
#include <device/usbd.h>
#include <kernel/types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
//-----------Declarations start here ---------------------

/**
    \brief The human interface device descriptor information.

    The hid descriptor structure defined in the USB HID 1.11 manual in 6.2.1.
*/

struct __attribute__((__packed__)) UsbDescriptorHeader1 {
    uint8_t DescriptorLength;               // +0x0
    enum DescriptorType DescriptorType : 8; // +0x1
};

struct __attribute__((__packed__)) HidDescriptor {
    struct UsbDescriptorHeader1 Header;
    union { // Place a union over BCD version .. alignment issues on ARM7/8
        struct __attribute__((__packed__, aligned(1))) {
            uint8_t HidVersionLo; // Lo of BCD version
            uint8_t HidVersionHi; // Hi of BCD version
        };
        uint16_t HidVersion; // (bcd version) +0x2
    };
    enum HidCountry {
        CountryNotSupported = 0,
        Arabic              = 1,
        Belgian             = 2,
        CanadianBilingual   = 3,
        CanadianFrench      = 4,
        CzechRepublic       = 5,
        Danish              = 6,
        Finnish             = 7,
        French              = 8,
        German              = 9,
        Greek               = 10,
        Hebrew              = 11,
        Hungary             = 12,
        International       = 13,
        Italian             = 14,
        Japan               = 15,
        Korean              = 16,
        LatinAmerican       = 17,
        Dutch               = 18,
        Norwegian           = 19,
        Persian             = 20,
        Poland              = 21,
        Portuguese          = 22,
        Russian             = 23,
        Slovakian           = 24,
        Spanish             = 25,
        Swedish             = 26,
        SwissFrench         = 27,
        SwissGerman         = 28,
        Switzerland         = 29,
        Taiwan              = 30,
        TurkishQ            = 31,
        EnglishUk           = 32,
        EnglishUs           = 33,
        Yugoslavian         = 34,
        TurkishF            = 35,
    } Countrycode : 8;            // +0x4
    uint8_t DescriptorCount;      // +0x5
    enum DescriptorType Type : 8; // 0x6
    union {                       // Place a union over length .. alignment issues on ARM7/8
        struct __attribute__((__packed__, aligned(1))) {
            uint8_t LengthLo; // Lo of Length
            uint8_t LengthHi; // Hi of Length
        };
        uint16_t Length; // +0x7
    };
};

/**
    \brief The possible types of hid reports.

    The possible hid reports defined in the USB HID 1.11 manual in 7.2.1.
*/
enum HidReportType {
    Input   = 1,
    Output  = 2,
    Feature = 3,
};

/** The DeviceDriver field in UsbDriverDataHeader for hid devices. */
#define DeviceDriverHid 0x48494430

/**
    \brief Hid specific data.

    The contents of the driver data field for hid devices. Chains to
    allow a stacked driver.
*/
struct HidDevice {
    struct UsbDriverDataHeader Header;
    struct HidDescriptor* Descriptor;
    struct HidParserResult* ParserResult;
    struct UsbDriverDataHeader* DriverData;

    uint8_t pollingIntervalInMs;

    // HID event handlers
    void (*HidDetached)(struct UsbDevice* device);
    void (*HidDeallocate)(struct UsbDevice* device);
};

#define HidUsageAttachCount 10

/**
    \brief Methods to attach an interface of particular hid desktop usage.

    The application desktop usage of the interface is the index into this array
    of methods. The array is populated by ConfigurationLoad().
*/
extern Result (*HidUsageAttach[HidUsageAttachCount])(struct UsbDevice* device, uint32_t interfaceNumber);

/**
    \brief Retrieves a hid report.

    Performs a hid get report request as defined in  in the USB HID 1.11 manual
    in 7.2.1.
*/
Result HidGetReport(struct UsbDevice* device, enum HidReportType reportType, uint8_t reportId, uint8_t interface,
                    uint32_t bufferLength, void* buffer);

/**
    \brief Sends a hid report.

    Performs a hid set report request as defined in  in the USB HID 1.11 manual
    in 7.2.2.
*/
Result HidSetReport(struct UsbDevice* device, enum HidReportType reportType, uint8_t reportId, uint8_t interface,
                    uint32_t bufferLength, void* buffer);

/**
    \brief Updates the device with the values of a report.

    Writes back the current values of a report in memory to the device.
    Implemented using HidSetReport, not interrupts.
*/
Result HidWriteDevice(struct UsbDevice* device, uint8_t report);

/**
    \brief Updates a report with the values from the device.

    Reads the current values of a report from the device into memory. Implemented
    using HidGetReport not interrupts.
*/
Result HidReadDevice(struct UsbDevice* device, uint8_t report);

/**
    \brief Enumerates a device as a HID device.

    Checks a device to see if it appears to be a HID device, and, if so, loads
    its hid and report descriptors to see what it can do.
*/
Result HidAttach(struct UsbDevice* device, uint32_t interfaceNumber);

void HidLoad();

void HidDeallocate(struct UsbDevice* device);

void HidDetached(struct UsbDevice* device);

Result HidSetProtocol(struct UsbDevice* device, uint8_t interface, uint16_t protocol);

Result HidParseReportDescriptor(struct UsbDevice* device, void* descriptor, uint16_t length);

void HidEnumerateReport(void* descriptor, uint16_t length, void (*action)(void* data, uint16_t tag, uint32_t value),
                        void* data);

void HidEnumerateActionCountReport(void* data, uint16_t tag, uint32_t value);

void HidEnumerateActionCountField(void* data, uint16_t tag, uint32_t value);

void HidEnumerateActionAddField(void* data, uint16_t tag, uint32_t value);

int32_t BitGetSigned(void* buffer, uint32_t offset, uint32_t length);

uint32_t BitGetUnsigned(void* buffer, uint32_t offset, uint32_t length);

void BitSet(void* buffer, uint32_t offset, uint32_t length, uint32_t value);
//-----------Declarations end here ---------------------
#ifdef __cplusplus
}
#endif

#endif