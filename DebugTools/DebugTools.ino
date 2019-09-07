#include "config.h"
#include "credentials.h"

#if WITH_GDB_STUB
#include <GDBStub.h>
#endif

/*
#include <WiFiServerSecure.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <BearSSLHelpers.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266WiFiAP.h>
#include <CertStoreBearSSL.h>
#include <WiFiUdp.h>
*/
#include <ESP8266WiFi.h>
/*
#include <WiFiServer.h>
#include <WiFiServerSecureAxTLS.h>
#include <WiFiClientSecureAxTLS.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiServerSecureBearSSL.h>
*/

extern "C"{

#define MEMLEAK_DEBUG
#include "mem.h"
bool ICACHE_FLASH_ATTR check_memleak_debug_enable()
{
  return MEMLEAK_DEBUG_ENABLE;
}


void custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end) {
  (void)rst_info;
  (void)stack;
  (void)stack_end;

  // dump the umm heap structures
  //wdt_reset();
  //umm_info(NULL,5);

  // dump the ROM mem_malloc stats
  //Serial.printf("Free Heap: %u\n\n", system_get_free_heap_size());

  Serial.printf("I lived for %ld minutes!\n", millis()/(60*1000));
  #if WIFI_PROMISC || PHY_CAPTURE
  phy_stats();
  #endif
  
  //WiFi.printDiag(Serial);
  WiFi.disconnect();    // try to be stabe enough to dump data

  #if INTR_LOCK_TEST
     print_intr_lock_stats();
  #endif // INTR_LOCK_TEST
     print_test_counters();

  #if STATE_CAPTURE_US
  print_states();
  #endif
  
  // getting tired of it crashing so often I can't read the crash report!
  // (and the scrollback buffer isn't long enough)
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("\n>>>>STOP<<<<");
  while (1) { wdt_reset(); }
}

}

#include "coredecls.h"  // for disable_extra4k...

unsigned int last, nextInfo;
int led;
extern int doprint_allow;


//extern "C" void ets_install_uart_printf(void);  // restore putc1 handler back to ROM version
//extern "C" void uart_buff_switch(uint8_t);  // Select UART: 0 => Serial, and 1 => Serial1
//extern int umm_debug_log_level;


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  //Serial.begin(74880); 
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  //umm_debug_log_level=0; // 0-6
  system_set_os_print(1);
  //disable_extra4k_at_link_time();
  
  struct rst_info* rst = system_get_rst_info();
  const char *reasons[]={"Normal Startup","Hardware WDT","Exception","Software WDT","Soft Restart", "Deep Sleep Awake", "External System Reset"};
  
  Serial.printf("Reset info: reason=%d %s,\n"
                "            exccause=%d,  excvaddr=%08x,\n"
                "            epc1=%08x, epc2=%08x, epc3=%08x, depc=%08x\n",
      rst->reason, reasons[rst->reason], 
      rst->exccause, rst->excvaddr,
      rst->epc1, rst->epc2, rst->epc3, rst->depc);

  if ((rst->reason==1) || (rst->reason==3) || (rst->reason==4)) {
     //if (rst->reason==4)
     //WiFi.printDiag(Serial);
     Serial.print(">>>>>STOP<<<<<\n");
     while (1) wdt_reset();
  }

  //do_memory_tests();
  
  #if WITH_GDB_STUB
  gdbstub_init();
  #endif
  
  #if FS_TEST
  spiffs_test();
  #endif

  Serial.printf("FLASH_SIZE_MAP=%d\n", system_get_flash_size_map());

#if STATE_CAPTURE_US
  Serial.print("Init States\n");
  init_states();
#endif
  
#if PHY_CAPTURE
  Serial.print("Grabbing phy_capture\n");
  phy_capture_init();
#endif

  //Serial.printf("Free Heap: %u\n", system_get_free_heap_size());
  
#if 0
#ifdef DEBUG_ESP_OOM
  Serial.printf("DEBUG_ESP_OOM is on\n");
#else
  Serial.printf("DEBUG_ESP_OOM is off\n");
#endif
#endif

#if WIFI_ENABLE
  Serial.print("Enabling WiFi\n");
  //WiFi.disconnect();
  //WiFi.forceSleepWake();
  //delay(100);

  WiFi.persistent(false);
  WiFi.setOutputPower(20.5); // 0-20.5, 17.75 max ok
  WiFi.setSleepMode(SLEEP_MODE, 0);  // NONE/MODEM/LIGHT, 0-10 (interval is SDK3 only)
  
  #if WIFI_PROMISC==0
  #if WIFI_ENABLE==2
     // AP mode
     IPAddress ip(192,168,4,1);
     IPAddress gateway(192,168,4,1);
     IPAddress subnet(255,255,255,0);
     if (!WiFi.mode(WIFI_AP)) {
        Serial.print("Can't set WiFi AP Mode\n");
     }
     WiFi.softAPConfig(ip,gateway,subnet);
     WiFi.softAP("ESP-AP","ESP-AP");
     Serial.print("SoftAP IP = ");
     Serial.println(WiFi.softAPIP());
  #else // WIFI_ENABLE==2
     // STA mode
     if (!WiFi.mode(WIFI_STA)) {
        Serial.printf("Can't set WiFi Station mode\n");
     }
     WiFi.hostname("ESP-DebugTools");
     #if NO_DHCP
       IPAddress ip(FIXED_IP_ADDR);
       IPAddress gateway(GATEWAY_IP_ADDR);
       IPAddress subnet(SUBNET_MASK);
       WiFi.config(ip,gateway,subnet);
     #else 
       WiFi.config(IPAddress(), IPAddress(), IPAddress());
     #endif
     WiFi.begin(STA_SSID, STA_PSK);

     Serial.print("Connecting WiFi");
     while (WiFi.status() != WL_CONNECTED) {
         Serial.print(".");
         delay(960);
         digitalWrite(LED_BUILTIN,LOW);
         delay(40);
         digitalWrite(LED_BUILTIN,HIGH);
         wdt_reset();
     }
  #endif // WIFI_ENABLE==2
  
  Serial.print("OK\n");

  #else // WIFI_PROMISC>0
    Serial.print("Enabling wifi promiscuous mode\n");
    WiFi.printDiag(Serial);
    WiFi.disconnect();
    wifi_promisc_init(WIFI_PROMISC);
  #endif // WIFI_PROMISC
#else
  WiFi.disconnect();
#endif

  last=millis();
  nextInfo=last+(5*60*1000);
  led=0;

#if MEM_FRAG_TEST
  mem_frag_init();
#endif

#if INTR_LOCK_TEST
  print_intr_lock_stats();
#endif
  print_test_counters();
}



void loop() {
  delay(10);
  wdt_reset();

  if (millis() > (last+1000)) {
     Serial.write(".");
     digitalWrite(LED_BUILTIN, led & 1 ? HIGH : LOW);

     #if MEM_FRAG_TEST
        mem_frag_buffers(true);
     #endif

     led++;
     if (led>=40) {
        Serial.write("\n");
        led=0;
     }
     last = millis();
  }
  if (millis() > nextInfo) {
     Serial.printf("\nAlive for %ld minutes!\n", millis()/(60*1000));
     //umm_info(NULL,5);
     //Serial.println();
     //system_print_meminfo();
     //Serial.println();
     #if WIFI_PROMISC || PHY_CAPTURE
     phy_stats();
     #endif
     Serial.println();

#if INTR_LOCK_TEST
     print_intr_lock_stats();
#endif // INTR_LOCK_TEST
     print_test_counters();
     
     nextInfo=millis()+(5*60*1000);
  }
}
