#ifndef _BCM_4343_H_
#define _BCM_4343_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <device/etherif.h>
#include <device/etherevent.h>
#include <device/netif.h>
#include <stdbool.h>
#include <stdint.h>

typedef int ether_pnp_t(Ether* edev);
typedef enum TAuthMode { AuthModeNone, AuthModeWPA, AuthModeWPA2 } TAuthMode;

typedef struct {
    NetDevice* net_device;
    Ether* edev;
    uint32_t is_used;
    TAuthMode auth_mode;
    char essid[128];
    char auth_cmd[128];
    char firmware_path[128];
} bcm4343_net_device;

bcm4343_net_device* allocate_bcm4343_device();
bcm4343_net_device* get_bcm4343_device();
void set_essid(bcm4343_net_device* net_device, char* essid);
void set_auth(bcm4343_net_device* net_device, TAuthMode Mode, char* pKey);
bool initialize(bcm4343_net_device* net_device);
const uint8_t* get_mac_address(bcm4343_net_device* net_device);
bool send_frame(bcm4343_net_device* net_device, const void* frame_buffer, uint32_t length);
bool wifi_control(bcm4343_net_device* net_device, const char *pFormat, ...);
void register_event_handler(bcm4343_net_device* net_device, ether_event_handler_t *pHandler, void *pContext);

void scan_result_received(const void *pBuffer, unsigned nLength);
bool receive_scan_results(void *pBuffer, unsigned *pResultLength);



// pBuffer must have size FRAME_BUFFER_SIZE
bool receive_frame(bcm4343_net_device* net_device, void* frame_buffer, uint32_t* length);
void etheriq(Ether* pEther, Block* pBlock, uint32_t nFlag);

// returns TRUE if PHY link is up
bool is_link_alive(bcm4343_net_device* net_device);
void dump_status(bcm4343_net_device* net_device);

#define NORETURN	__attribute__ ((noreturn))
void assertion_failed (const char *pExpr, const char *pFile, unsigned nLine) NORETURN;

	#define assert(expr)	(  likely (expr)	\
				 ? ((void) 0)		\
				 : assertion_failed (#expr, __FILE__, __LINE__))
#ifdef __cplusplus
}
#endif

#endif
