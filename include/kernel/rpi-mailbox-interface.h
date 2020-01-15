
#ifndef RPI_MAILBOX_INTERFACE_H
#define RPI_MAILBOX_INTERFACE_H

#include <stdint.h>

/**
    @brief An enum of the RPI->Videocore firmware mailbox property interface
    properties. Further details are available from
    https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
*/
typedef enum
{
    /* Videocore */
    TAG_GET_FIRMWARE_VERSION = 0x1,

    /* Hardware */
    TAG_GET_BOARD_MODEL = 0x10001,
    TAG_GET_BOARD_REVISION,
    TAG_GET_BOARD_MAC_ADDRESS,
    TAG_GET_BOARD_SERIAL,
    TAG_GET_ARM_MEMORY,
    TAG_GET_VC_MEMORY,
    TAG_GET_CLOCKS,

    /* Config */
    TAG_GET_COMMAND_LINE = 0x50001,

    /* Shared resource management */
    TAG_GET_DMA_CHANNELS = 0x60001,

    /* Power */
    TAG_GET_POWER_STATE = 0x20001,
    TAG_GET_TIMING,
    TAG_SET_POWER_STATE = 0x28001,

    /* Clocks */
    TAG_GET_CLOCK_STATE = 0x30001,
    TAG_SET_CLOCK_STATE = 0x38001,
    TAG_GET_CLOCK_RATE = 0x30002,
    TAG_SET_CLOCK_RATE = 0x38002,
    TAG_GET_MAX_CLOCK_RATE = 0x30004,
    TAG_GET_MIN_CLOCK_RATE = 0x30007,
    TAG_GET_TURBO = 0x30009,
    TAG_SET_TURBO = 0x38009,

    TAG_EXECUTE_QPU = 0x00030011, // Execute code on QPU
    TAG_ENABLE_QPU = 0x00030012, // QPU enable

    /* Memory commands */
    TAG_ALLOCATE_MEMORY = 0x0003000C, // Allocate Memory
    TAG_LOCK_MEMORY = 0x0003000D, // Lock memory
    TAG_UNLOCK_MEMORY = 0x0003000E, // Unlock memory
    TAG_RELEASE_MEMORY = 0x0003000F, // Release Memory
    
    TAG_EXECUTE_CODE = 0x30010,
    TAG_GET_DISPMANX_MEM_HANDLE = 0x30014,
    TAG_GET_EDID_BLOCK = 0x30020,

    /* Voltage */
    TAG_GET_VOLTAGE = 0x30003,
    TAG_SET_VOLTAGE = 0x38003,
    TAG_GET_MAX_VOLTAGE = 0x30005,
    TAG_GET_MIN_VOLTAGE = 0x30008,
    TAG_GET_TEMPERATURE = 0x30006,
    TAG_GET_MAX_TEMPERATURE = 0x3000A,

    /* GPIO */
	TAG_GET_DOMAIN_STATE = 0x30030,
	TAG_GET_GPIO_STATE = 0x30041,
	TAG_SET_GPIO_STATE = 0x38041,

	TAG_GET_GPIO_CONFIG = 0x30043,
	TAG_SET_GPIO_CONFIG = 0x38043,

	TAG_SET_CUSTOMER_OTP = 0x38021,
	TAG_SET_DOMAIN_STATE = 0x38030,
	TAG_SET_SDHOST_CLOCK = 0x38042,
    
    /* Framebuffer */
    TAG_ALLOCATE_BUFFER = 0x40001,
    TAG_RELEASE_BUFFER = 0x48001,
    TAG_BLANK_SCREEN = 0x40002,
    TAG_GET_PHYSICAL_SIZE = 0x40003,
    TAG_TEST_PHYSICAL_SIZE = 0x44003,
    TAG_SET_PHYSICAL_SIZE = 0x48003,
    TAG_GET_VIRTUAL_SIZE = 0x40004,
    TAG_TEST_VIRTUAL_SIZE = 0x44004,
    TAG_SET_VIRTUAL_SIZE = 0x48004,
    TAG_GET_DEPTH = 0x40005,
    TAG_TEST_DEPTH = 0x44005,
    TAG_SET_DEPTH = 0x48005,
    TAG_GET_PIXEL_ORDER = 0x40006,
    TAG_TEST_PIXEL_ORDER = 0x44006,
    TAG_SET_PIXEL_ORDER = 0x48006,
    TAG_GET_ALPHA_MODE = 0x40007,
    TAG_TEST_ALPHA_MODE = 0x44007,
    TAG_SET_ALPHA_MODE = 0x48007,
    TAG_GET_PITCH = 0x40008,
    TAG_GET_VIRTUAL_OFFSET = 0x40009,
    TAG_TEST_VIRTUAL_OFFSET = 0x44009,
    TAG_SET_VIRTUAL_OFFSET = 0x48009,
    TAG_GET_OVERSCAN = 0x4000A,
    TAG_TEST_OVERSCAN = 0x4400A,
    TAG_SET_OVERSCAN = 0x4800A,
    TAG_GET_PALETTE = 0x4000B,
    TAG_TEST_PALETTE = 0x4400B,
    TAG_SET_PALETTE = 0x4800B,
    TAG_SET_CURSOR_INFO = 0x8011,
    TAG_SET_CURSOR_STATE = 0x8010

} rpi_mailbox_tag_t;

typedef enum
{
    TAG_STATE_REQUEST = 0,
    TAG_STATE_RESPONSE = 1,
} rpi_tag_state_t;

typedef enum
{
    PT_OSIZE = 0,
    PT_OREQUEST_OR_RESPONSE = 1,
} rpi_tag_buffer_offset_t;

typedef enum
{
    T_OIDENT = 0,
    T_OVALUE_SIZE = 1,
    T_ORESPONSE = 2,
    T_OVALUE = 3,
} rpi_tag_offset_t;

typedef struct
{
    int32_t tag;
    int32_t byte_length;
    union {
        int32_t value_32;
        uint8_t buffer_8[256];
        int32_t buffer_32[64];
    } data;
} rpi_mailbox_property_t;

typedef enum
{
    TAG_CLOCK_RESERVED = 0,
    TAG_CLOCK_EMMC = 1,
    TAG_CLOCK_UART = 2,
    TAG_CLOCK_ARM = 3,
    TAG_CLOCK_CORE = 4,
    TAG_CLOCK_V3D = 5,
    TAG_CLOCK_H264 = 6,
    TAG_CLOCK_ISP = 7,
    TAG_CLOCK_SDRAM = 8,
    TAG_CLOCK_PIXEL = 9,
    TAG_CLOCK_PWM = 10,
} rpi_tag_clock_id_t;


extern void RPI_PropertyInit(void);
extern void RPI_PropertyAddTag(rpi_mailbox_tag_t tag, ...);
extern int32_t RPI_PropertyProcess(void);
extern rpi_mailbox_property_t *RPI_PropertyGet(rpi_mailbox_tag_t tag);

#endif