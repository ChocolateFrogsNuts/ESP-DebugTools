
#include "config.h"

#if FLASH_SPEEDTEST

#include "flash_hal.h"

const char *flashMode(uint8_t m) {
  switch (m) {
    case FM_QIO:  return "QIO";
    case FM_QOUT: return "QOUT";
    case FM_DIO:  return "DIO";
    case FM_DOUT: return "DOUT";
    case FM_UNKNOWN: return "UNKNOWN";
  }
  return "UNDEFINED";
}

void flash_speed_test() {
  // read-only direct flash speed test.
  // Reads 1MB of data from the flash chip

  uint32 addr;
  unsigned long total=0;
  int bs=FS_PHYS_BLOCK;
  uint32 *buf=(uint32 *)malloc(bs);

  Serial.printf("Running flash read speed test\n");
  Serial.printf("Chip ID:%08x\n", ESP.getFlashChipId());
  Serial.printf("CPU Freq:%d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Speed:%d MHz  %s\n", ESP.getFlashChipSpeed()/1000000, flashMode(ESP.getFlashChipMode()));
  Serial.printf("bs=%d\n", bs);
  unsigned long tstart=millis();

  uint32 startaddr= 0;
  uint32 endaddr  = 102400;
  
  for (int i=0; i<10; i++) {
    addr=startaddr;
    while (addr<endaddr) {
      if (!ESP.flashRead(addr, buf, bs)) {
         Serial.printf("ESP.flashRead() failed\n");
         return;
      }
      addr+=bs;
      total+=bs;
    }
  }

  unsigned long tend=millis();
  free(buf);
  Serial.printf("Read %ld bytes from flash in %ld ms = %f bytes/ms\n",
     total, tend - tstart, (double)total/(tend - tstart));
  Serial.printf("%d, %d %s, %ldms, %f\n", 
     ESP.getCpuFreqMHz(), ESP.getFlashChipSpeed()/1000000, 
     flashMode(ESP.getFlashChipMode()),
     tend - tstart, (double)total/(tend - tstart));
}

#endif // FLASH_SPEEDTEST
