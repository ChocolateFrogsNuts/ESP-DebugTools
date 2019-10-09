#include "config.h"

#if WITH_IRAMTEST

void iram_test() {
  uint32_t dport0 = ESP8266_DREG(0x024);
  Serial.printf("dport0 = 0x%08x  (iram2=%s, iram3=%s)\n", 
      dport0, dport0 & (1<<4) ? "off" : "on", dport0 & (1<<3) ? "off" : "on");

  // map in the extra 2x16K iram
  //Serial.printf("iram 2\n");
  //ESP8266_DREG(0x24) &= ~(1<<4);
  //Serial.printf("iram 3\n");
  //ESP8266_DREG(0x24) &= ~(1<<3);

  /*  0x40100000 == iram0
   *  0x40108000 == iram2
   *  0x4010C000 == iram3
   */
  //uint32_t *p = (uint32_t *)_spi0_command;
  volatile uint32_t *p = (uint32_t *)0x40100000;
  Serial.print("iram dump:\n");
  for (int i=0; i<256/4; i++) {
      uint32_t x = *p;
      if (i%8==0) Serial.printf("\n%p:", p);
      Serial.printf(" %08x",x);
      p++;
  }
  Serial.print('\n');

  Serial.print("iram write test:\n");
  p=(uint32_t *)0x40100000;
  uint32_t tmp = *p;
  *p=0xAFAFAFAF;
  Serial.printf("Read back %08x\n",*p);
  *p=tmp;
}

#endif // IRAM_TEST
