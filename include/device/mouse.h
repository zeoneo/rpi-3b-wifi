#ifndef _MOUSE_H
#define _MOUSE_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/types.h>
#include <device/hid.h>
#include <device/usb_report.h>
#include <device/usb-mem.h>
#include <plibc/stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif
//-----------Declarations start here ---------------------

/** The DeviceDriver field in UsbDriverDataHeader for mouse devices. */
#define DeviceDriverMouse 0x4B424431
/** The maximum number of keys a mouse can report at once. Should be 
	multiple of 2. */
#define MouseMaxKeys 6

enum MouseDeviceButton {
	MouseDeviceButtonLeft,
	MouseDeviceButtonRight,
	MouseDeviceButtonMiddle,
	MouseDeviceButtonSide,
	MouseDeviceButtonExtra,
};

/** 
	\brief Mouse specific data.

	The contents of the driver data field for mouse devices. Placed in
	HidDevice, as this driver is bult atop that.
*/
struct MouseDevice {
	/** Standard driver data header. */
	struct UsbDriverDataHeader Header;
	/** Internal - Index in mouse arrays. */
	uint32_t Index;

	uint8_t buttonState;
	int16_t mouseX;
	int16_t mouseY;
	int16_t wheel;

	/** The input report. */
	struct HidParserReport *MouseReport;
};

void MouseLoad();

/**
	\brief Enumerates a device as a mouse.

	Checks a device already checked by HidAttach to see if it appears to be a 
	mouse, and, if so, bulds up necessary information to enable the 
	mouse methods.
*/
Result MouseAttach(struct UsbDevice *device, uint32_t interface);

/**
	\brief Returns the number of mouses connected to the system.	
*/
uint32_t MouseCount();

/**
	\brief Checks a given mouse.

	Reads back the report from a given mouse and parses it into the internal
	fields. These can be accessed with MouseGet... methods
*/
Result MousePoll(uint32_t mouseAddress);

/** 
	\brief Returns the device address of the nth connected mouse.

	Mouses that are connected are stored in an array, and this method 
	retrieves the nth item from that array. Returns 0 on error.
*/
uint32_t MouseGetAddress(uint32_t index);

/**
	\brief Returns the current X coordinate of the mouse
*/
int16_t MouseGetPositionX(uint32_t mouseAddress);

/**
	\brief Returns the current Y coordinate of the mouse
*/
int16_t MouseGetPositionY(uint32_t mouseAddress);

/**
	\brief Returns the current wheel value of the mouse
*/
int16_t MouseGetWheel(uint32_t mouseAddress);

/**
	\brief Returns the current X and Y coordinates of the mouse

	X is in the high 16 bits, Y is in the low 16 bits
*/
uint32_t MouseGetPosition(uint32_t mouseAddress);

/**
	\brief Returns the current button state of the mouse

	First bit : Left button
	Second bit: Right button
	Third bit : Middle button
	Fourth bit: Side button
	Fifth bit : Extra button
*/
uint8_t MouseGetButtons(uint32_t mouseAddress);

/**
	\brief Returns whether or not a particular mouse button is pressed.

	Reads back whether or not a mouse button was pressed on the last 
	successfully received report.
*/
bool MouseGetButtonIsPressed(uint32_t mouseAddress, enum MouseDeviceButton button);


//-----------Declarations end here ---------------------
#ifdef __cplusplus
}
#endif

#endif