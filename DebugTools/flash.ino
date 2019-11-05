
#include "config.h"

#if FLASH_SPEEDTEST

#include <SPI.h>

#include "user_interface.h"

// for precache
#include "core_esp8266_features.h"

// for SPI0Command
#include "spi_utils.h"

/*
 * This file contains C translations of some of the ROM code for SPI flash chips
 * as well as a couple of extra funcs for sending user-defined SPI flash commands.
 */

#define memw() asm("memw");
extern "C" int Wait_SPI_Idle(SpiFlashChip *fc);

uint32_t SPI_read_status_(SpiFlashChip *fc, uint32_t *st) {
  do {
    SPI0RS=0;
    SPI0CMD=SPICMDRDSR;
    while (SPI0CMD) {}
  } while ((*st=(SPI0RS & fc->status_mask)) & SPIRSBUSY);
  return SPI_FLASH_RESULT_OK;
}

uint32_t SPI_write_status_(SpiFlashChip *fc, uint32_t st) {
  Wait_SPI_Idle(fc);
  SPI0RS=st;
  SPI0CMD=SPICMDWRSR;
  while (SPI0CMD) {}
  return SPI_FLASH_RESULT_OK;
}

uint32_t SPI_read_(uint32_t addr, uint8_t *dst, uint32_t len) {
  if (len==0) return SPI_FLASH_RESULT_OK;
  Wait_SPI_Idle(flashchip);
  uint8_t *src = (uint8_t *)&(SPI0W0);
  if (len<=32) {
     // translated from asm - rom does small xfers like this
     SPI0A = addr | (len<<24);
     SPI0CMD = SPICMDREAD;
     while (SPI0CMD) {}
     memcpy(dst, src, len);
  } else {
     // not translated from asm - rom may do this differently.
     // ie it may support DMA or some other bulk xfer mechanism.
     while (len>32) {
        SPI0A = addr | (32<<24);
        SPI0CMD = SPICMDREAD;
        while (SPI0CMD) {}
        memcpy(dst, src, len);
        addr+=32;
        dst+=32;
        len-=32;
     }
     SPI0A = addr | (len<<24);
     SPI0CMD = SPICMDREAD;
     while (SPI0CMD) {}
     memcpy(dst, src, len);
  }
  return SPI_FLASH_RESULT_OK;
}

// ROM functions not in user_interface.h
extern "C" void SelectSpiFunction();
extern "C" void spi_flash_attach();
extern "C" SpiFlashOpResult SPI_read_status(SpiFlashChip *fc, uint32_t *st);
extern "C" SpiFlashOpResult SPI_write_status(SpiFlashChip *fc, uint32_t st);
extern "C" uint32_t SPIParamCfg(uint32_t deviceId, uint32_t chip_size, uint32_t block_size, uint32_t sector_size, uint32_t page_size, uint32_t status_mask);

#ifndef SPI_FLASH_VENDOR_XMC
#define SPI_FLASH_VENDOR_XMC 0x20
#endif

#define SPI_FLASH_XMC_DRV_25  1
#define SPI_FLASH_XMC_DRV_50  0
#define SPI_FLASH_XMC_DRV_75  2
#define SPI_FLASH_XMC_DRV_100 3

#define SPI_FLASH_XMC_DRV_S   5
#define SPI_FLASH_XMC_DRV_MASK 0x03

#define SPI_FLASH_RSR1  0x05
#define SPI_FLASH_RSR2  0x35
#define SPI_FLASH_RSR3  0x15
#define SPI_FLASH_WSR1  0x01
#define SPI_FLASH_WSR2  0x31
#define SPI_FLASH_WSR3  0x11
#define SPI_FLASH_WEVSR 0x50
#define SPI_FLASH_WREN  0x06
#define SPI_FLASH_WRDI  0x04

void flash_xmc_check() {
  using namespace experimental;
  if (ESP.getFlashChipVendorId() == SPI_FLASH_VENDOR_XMC) {

     #if 0
     Serial.printf("flashchip: ID:%08x, smask:%08x\n", flashchip->deviceId, flashchip->status_mask);
     int rc=SPIParamCfg(ESP.getFlashChipId(), flashchip->chip_size,
                        flashchip->block_size, flashchip->sector_size,
                        flashchip->page_size, 0x00ffffff);
     Serial.printf("flashchip: ID:%08x, smask:%08x,  rc=%d\n", flashchip->deviceId, flashchip->status_mask, rc);
     #endif

     // test assorted flash access functions
     uint32_t cfg;
     cfg=0;
     if (spi_flash_read(0x0000, &cfg, 4) != SPI_FLASH_RESULT_OK) {
        Serial.print("spi_flash_read failed\n");
     }
     Serial.printf("spi_flash_read: first 4 bytes of flash=%08x\n",cfg);

     cfg=0;
     if (SPI_read_(0x0000, (uint8_t*)&cfg, 4) != SPI_FLASH_RESULT_OK) {
        Serial.print("SPI_read_ failed\n");
     }
     Serial.printf("SPI_read_: first 4 bytes of flash=%08x\n",cfg);
     
     cfg=0;
     if (SPI0Command(0x03, &cfg, 24, 32)!=SPI_RESULT_OK) { // 0x03 = Read Data
        Serial.print("SPI0Command: read flash failed\n");
     }
     Serial.printf("SPI0Command: first 4 bytes of flash=%08x\n",cfg);

     uint32_t faddr = 16;
     uint8_t fdata[64];
     memset(fdata,0,sizeof(fdata));
     if (spi_flash_read(16, (uint32_t*)fdata, sizeof(fdata))!=SPI_FLASH_RESULT_OK) {
        Serial.print("SPI0Command: read flash failed\n");
     }
     Serial.printf("spi_flash_read: bytes %d - %d of flash:", faddr, faddr+sizeof(fdata));
     for (unsigned int i=0; i<sizeof(fdata); i++) {
         if (i % 16 == 0) Serial.print("\n");
         Serial.printf("%02x ",fdata[i]);
     }
     Serial.print("\n");

     memset(fdata,0,sizeof(fdata));
     fdata[2]=faddr & 0xFF; // address needs to be big-endian, 24-bit
     fdata[1]=(faddr>>8) & 0xFF;
     fdata[0]=(faddr>>16) & 0xFF;
     if (SPI0Command(0x03, (uint32_t*)fdata, 24, sizeof(fdata)*8)!=SPI_RESULT_OK) { // 0x03 = Read Data
        Serial.print("SPI0Command: read flash failed\n");
     }
     Serial.printf("SPI0Command: bytes %d - %d of flash:", faddr, faddr+sizeof(fdata));
     for (unsigned int i=0; i<sizeof(fdata); i++) {
         if (i % 16 == 0) Serial.print("\n");
         Serial.printf("%02x ",fdata[i]);
     }
     Serial.print("\n");

     uint32_t SR=0, SR1=0,SR2=0,SR3, newSR3;
     #if 0
     Serial.print("SPI_Read SR\n");
     if (SPI_read_status(flashchip, &SR) != SPI_FLASH_RESULT_OK) {
        Serial.print("SPI_read_status error\n");
     }
     Serial.printf("SPI_read_status SR=%08x\n", SR);

     if (SPI_read_status_(flashchip, &SR) != SPI_FLASH_RESULT_OK) {
        Serial.print("SPI_read_status_ error\n");
     }
     Serial.printf("SPI_read_status_ SR=%08x\n", SR);

     Serial.print("Read SR1\n");
     if (SPI0Command(SPI_FLASH_RSR1, &SR1, 0, 8)!=SPI_RESULT_OK) {
        Serial.printf("SPI0Command(read SR1) failed\n");
     }
     Serial.print("Read SR2\n");
     if (SPI0Command(SPI_FLASH_RSR2, &SR2, 0, 8)!=SPI_RESULT_OK) {
        Serial.printf("SPI0Command(read SR2) failed\n");
     }
     #endif
     
     Serial.print("Read SR3\n");
     if (SPI0Command(SPI_FLASH_RSR3, &SR3, 0, 8)!=SPI_RESULT_OK) {
        Serial.printf("SPI0Command(read SR3) failed\n");
     }
     
     Serial.printf("XMC Flash found, SR1=%02x, SR2=%02x, SR3=%02x\n", SR1,SR2,SR3);
     newSR3=SR3;
     
     // XMC chips default to 75% drive on their outputs during read,
     // but can be switched to 100% by setting SR3:DRV0,DRV1 to 1,1.
     // Only needed if we are trying to run >26MHz.
     int ffreq = ESP.getFlashChipSpeed()/1000000;
     if (ffreq > 26) {
        newSR3 &= ~(SPI_FLASH_XMC_DRV_MASK << SPI_FLASH_XMC_DRV_S);
        newSR3 |= (SPI_FLASH_XMC_DRV_75 << SPI_FLASH_XMC_DRV_S);
     }

     // Additionally they have a high-frequency mode that holds pre-charge on the
     // internal charge pump, keeping the access voltage more readily available.
     // Only for QPI mode (which we don't use)
     if (ffreq > 40) {
        // SR3:HFM=1
        // newSR3|=0x10;
     }

     Serial.printf("New SR3=%02x\n", newSR3);
     #if 1
     if (newSR3 != SR3) {
        #if 1
          Serial.print("WEVSR\n");
          if (SPI0Command(SPI_FLASH_WEVSR,NULL,0,0)!=SPI_RESULT_OK) {
             Serial.print("SPI0Command(write volatile enable) failed\n");
          }
          Serial.print("WSR3\n");
          if (SPI0Command(SPI_FLASH_WSR3,&newSR3,8,0)!=SPI_RESULT_OK) {
             Serial.print("SPI0Command(write SR3) failed\n");
          }
          Serial.print("WRDI\n");
          if (SPI0Command(SPI_FLASH_WRDI,NULL,0,0)!=SPI_RESULT_OK) {
             Serial.print("SPI0Command(write disable) failed\n");
          }
          Serial.print("RSR3\n");
          if (SPI0Command(SPI_FLASH_RSR3, &SR3, 0, 8)!=SPI_RESULT_OK) {
             Serial.printf("SPI0Command(re-read SR3) failed\n");
          }
        #else
          SR = (SR & 0xffff) | (newSR3<<16);
          if (SPI_write_status(flashchip, SR) != SPI_FLASH_RESULT_OK) {
             Serial.print("SPI_writestatus error\n");
          }
          if (SPI_read_status(flashchip, &SR) != SPI_FLASH_RESULT_OK) {
             Serial.print("SPI_read_status error\n");
          }
        #endif
        Serial.printf("Updated SR3=%08x\n",SR3);
     }
     #endif
  }

}


/* flash_init_quirks()
 * Do any chip-specific initialization to improve performance.
 */
void flash_init_quirks() {
  using namespace experimental;
  switch (ESP.getFlashChipVendorId()) {
    case SPI_FLASH_VENDOR_XMC:
         uint32_t SR3, newSR3;
         if (SPI0Command(SPI_FLASH_RSR3, &SR3, 0, 8)==SPI_RESULT_OK) { // read SR3
            newSR3=SR3;
            if (ESP.getFlashChipSpeed()>26000000) { // >26Mhz?
               newSR3 &= ~(SPI_FLASH_XMC_DRV_MASK << SPI_FLASH_XMC_DRV_S);
               newSR3 |= (SPI_FLASH_XMC_DRV_100 << SPI_FLASH_XMC_DRV_S);
            }
            if (newSR3 != SR3) { // only write if changed
               if (SPI0Command(SPI_FLASH_WEVSR,NULL,0,0)==SPI_RESULT_OK)  // write enable volatile SR
                  SPI0Command(SPI_FLASH_WSR3,&newSR3,8,0);  // write to SR3
               SPI0Command(SPI_FLASH_WRDI,NULL,0,0);        // write disable - probably not needed
            }
         }
  }
}

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

#include "esp8266_peri.h"


// copy_raw - copied from the eboot bootloader.
int copy_raw(const uint32_t src_addr,
             const uint32_t dst_addr,
             const uint32_t size)
{
  if ((src_addr & 0xfff) != 0 || (dst_addr & 0xfff) != 0)
     return 1;

  const uint32_t buffer_size = flashchip->sector_size;
  uint8_t *buffer = (uint8_t *)malloc(buffer_size);
  uint32_t left = (size+buffer_size-1) & ~(buffer_size-1);
  uint32_t saddr = src_addr;
  uint32_t daddr = dst_addr;
  
  while (left) {
        if (SPIEraseSector(daddr/buffer_size)) {
            free(buffer);
            return 2;
        }
        if (SPIRead(saddr, buffer, buffer_size)) {
            free(buffer);
            return 3;
        }
        if (SPIWrite(daddr, buffer, buffer_size)) {
            free(buffer);
            return 4;
        }
        ESP.wdtFeed();
        saddr += buffer_size;
        daddr += buffer_size;
        left  -= buffer_size;
  }
  free(buffer);
  return 0;
}

int compare_raw(const uint32_t src1_addr,
                const uint32_t src2_addr,
                const uint32_t size)
{
  if ((src1_addr & 0xfff) != 0 || (src2_addr & 0xfff) != 0)
     return 1;

  const uint32_t buffer_size = flashchip->sector_size;
  uint8_t *buffer1 = (uint8_t *)malloc(buffer_size);
  uint8_t *buffer2 = (uint8_t *)malloc(buffer_size);
  
  uint32_t left = (size+buffer_size-1) & ~(buffer_size-1);
  uint32_t s1addr = src1_addr;
  uint32_t s2addr = src2_addr;
  
  while (left) {
        if (SPIRead(s1addr, buffer1, buffer_size)) {
            free(buffer1);
            free(buffer2);
            return 3;
        }
        if (SPIRead(s2addr, buffer2, buffer_size)) {
            free(buffer1);
            free(buffer2);
            return 4;
        }
        if (memcmp(buffer1, buffer2, buffer_size)) {
            free(buffer1);
            free(buffer2);
            return 5;            
        }
        ESP.wdtFeed();
        s1addr += buffer_size;
        s2addr += buffer_size;
        left  -= buffer_size;
  }
  free(buffer1);
  free(buffer2);
  return 0;
}

void PRECACHE_ATTR flash_speed_test() {
  // read-only direct flash speed test.
  // Reads 1MB of data from the flash chip

  flash_xmc_check();

  uint32 addr;
  unsigned long total=0;
  int bs=flashchip->sector_size;
  uint32 *buf=(uint32 *)malloc(bs);

  Serial.printf("\nCPU Freq:%d MHz, Flash:%d MHz  %s\n", 
      ESP.getCpuFreqMHz(), ESP.getFlashChipSpeed()/1000000, flashMode(ESP.getFlashChipMode()));
  Serial.printf("SPI Flash Parameters (flashchip->*):\n   ID:%08x, size:%d, bsize:%d, ssize:%d, psize:%d, smask:%08x\n",
      flashchip->deviceId, flashchip->chip_size, flashchip->block_size,
      flashchip->sector_size, flashchip->page_size, flashchip->status_mask);
  Serial.printf("ESP.get* :\n   ID:%08x, size:%d, realSize:%d\n", 
      ESP.getFlashChipId(), ESP.getFlashChipSize(), ESP.getFlashChipRealSize());
  Serial.printf("SPI0CLK: 0x%08x\n", SPI0CLK);
  Serial.printf("SPI0A : 0x%08x\n", SPI0A);
  Serial.printf("SPI0C : 0x%08x\n", SPI0C);
  Serial.printf("SPI0C1: 0x%08x\n", SPI0C1);
  Serial.printf("SPI0C2: 0x%08x\n", SPI0C2);
  Serial.printf("SPI0U : 0x%08x\n", SPI0U);
  Serial.printf("SPI0U1: 0x%08x\n", SPI0U1);
  Serial.printf("SPI0U2: 0x%08x\n", SPI0U2);
  Serial.printf("SPI0WS: 0x%08x\n", SPI0WS);
  Serial.printf("SPI0P : 0x%08x\n", SPI0P);
  Serial.printf("SPI0S : 0x%08x\n", SPI0S);
  Serial.printf("SPI0S1: 0x%08x\n", SPI0S1);
  Serial.printf("SPI0S2: 0x%08x\n", SPI0S2);
  Serial.printf("SPI0S3: 0x%08x\n", SPI0S3);
  Serial.printf("SPI0E3: 0x%08x\n", SPI0E3);
  Serial.printf("Running flash read speed test, bs=%d\n",bs);

  uint32 startaddr= 0;
  uint32 endaddr  = 102400;
  unsigned long tstart=millis();
  
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
  Serial.printf("%d, %d %s, %ldms, %f\n\n", 
     ESP.getCpuFreqMHz(), ESP.getFlashChipSpeed()/1000000, 
     flashMode(ESP.getFlashChipMode()),
     tend - tstart, (double)total/(tend - tstart));

  Serial.printf("Running flash copy speed test, bs=%d, Size=1MB\n",bs);
  tstart=-millis();
  const uint32_t MB = 1024*1024;

  uint32_t vendor = spi_flash_get_id() & 0x000000ff;

  uint32_t spi0clk = SPI0CLK;
  uint32_t spi0c   = SPI0C;

//if (vendor == SPI_FLASH_VENDOR_XMC) {
   uint32_t flashinfo=0;

  if (SPIRead(0, &flashinfo, 4)) {
     // fail
  }

#if 1
  // There are limits to how much we can slow down depending on the current speed.
  // This is a workaround (because we should be able to just use 20Mhz every time)
  switch ((flashinfo >> 24) & 0x0F) {
     case 0x0: // 40MHz, slow to 20
     case 0x1: // 26 mhz, slow to 20
             SPI0CLK = 0x00003043;
             SPI0C   = 0x00EAA313;
             break;
     case 0x2: // 20Mhz - no change
             break;
     case 0xf: // 80Mhz, slow to 26
             SPI0CLK = 0x00002002;
             SPI0C   = 0x00EAA202;
             break;
     default:
             break;
  }
#endif

#if 0
  switch (mhz) {
    case 20: SPI0CLK = 0x00003043;
             SPI0C   = 0x00EAA313;
             break;
    case 26: SPI0CLK = 0x00002002;
             SPI0C   = 0x00EAA202;
             break;
    case 40: SPI0CLK = 0x00001001;
             SPI0C   = 0x00EAA101;
             break;
    case 80: SPI0CLK = 0x80000000;
             SPI0C   = 0x00EAB000;
             break;
    default: break;
  }
#endif
//}
    
  int cprc = copy_raw(0*MB, 3*MB, 1*MB);

  Serial.printf("SPI0CLK=%08x\n", SPI0CLK);
  SPI0CLK = spi0clk;
  SPI0C   = spi0c;

  tend=millis();
  int cmprc = compare_raw(0*MB, 3*MB, 1*MB);
  Serial.printf("copy_raw returned %d after %ld ms\n", cprc, tend-tstart);
  Serial.printf("  %f bytes/ms,  cmp returned %d (%s)\n\n", (double)(1*MB)/(tend - tstart), 
     cmprc, cmprc ? "error" : "ok");
}

#endif // FLASH_SPEEDTEST
