
#ifndef _CONFIG_H_
#define _CONFIG_H_

// microseconds between capturing the stack state. 
//     0 == disable, 
//     1 == on request only
//  2-99 == not used (store_state does nothing)
// >=100 == capture interval
//#define STATE_CAPTURE_US 500
#define STATE_CAPTURE_US 1

#define MAXSTATES   27
#define MAXSTACK  (1024/4)  // in 32-bit words (ie 1024/4 = 1K)


// 1=allocate/fill/verify buffers in pseudo-random order/size
#define MEM_FRAG_TEST 0

// tcpdump like traffic capture
#define PHY_CAPTURE   1
#define PHY_CAPTURE_DUMP 1

// use promiscuous mode - disables normal traffic
// 0=off, other=wifi channel number
#define WIFI_PROMISC 0

// 0=no dump, 1=dump only unknown traffic, 2=dump all traffic
#define WIFI_PROMISC_DUMP 1

// Testing for the nesting depth and call sources of ets_intr_(un)lock
#define INTR_LOCK_TEST 0

// Testing SPIFFS
#define FS_TEST 0

#define FLASH_SPEEDTEST 1

#define WITH_MEMTEST 0

// include /initialize the GDB stub for debugging
#define WITH_GDB_STUB 0

// NONE doesn't crash so much
#define SLEEP_MODE WIFI_NONE_SLEEP
// MODEM crashes better
//#define SLEEP_MODE WIFI_MODEM_SLEEP

// 0=WiFi off, no crash
// 1=WiFi on, bad stack and crash
// 2=WiFi in AP Mode
#define WIFI_ENABLE 1

// 1=use fixed IP, not DHCP
#define NO_DHCP 1

///////// Config ends here ////////////

// Only one of these can work at a time
#if PHY_CAPTURE && WIFI_PROMISC
#undef PHY_CAPTURE
#define PHY_CAPTURE 0
#endif

#if STATE_CAPTURE_US
extern "C" void print_states();
extern "C" void init_states();
#endif

#if PHY_CAPTURE
extern "C" void phy_stats();
extern "C" void phy_capture_init();
#endif

#endif // _CONFIG_H_
