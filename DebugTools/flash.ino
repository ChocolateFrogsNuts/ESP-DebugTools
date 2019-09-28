
#include "config.h"

#if FLASH_SPEEDTEST

#include <SPI.h>

#include "user_interface.h"

#define memw() asm("memw");

ICACHE_RAM_ATTR void spi0_setDataLengths(uint8_t mosi_bits, uint8_t miso_bits) {
  #ifdef ESP32
    // placeholder to remind someone to fix this
    #error ESP32 not yet supported
  #else
    if (mosi_bits!=0) mosi_bits--;
    if (miso_bits!=0) miso_bits--;
    //Serial.printf("SPI0U1:=%08x\n", (miso_bits<<8)|(mosi_bits<<17));
    SPI0U1 = (miso_bits << 8) | (mosi_bits << 17);
  #endif
}

ICACHE_RAM_ATTR int spi0_command(uint8_t cmd, uint32_t *data, uint32_t data_bits, uint32_t read_bits) {
  if (data_bits>(64*8)) return 1;
  if (read_bits>(64*8)) return 1;

  uint32_t old_spi_usr = SPI0U;
  uint32_t old_spi_usr2= SPI0U2;

  uint32_t flags=(1<<31); //SPI_USR_COMMAND
  if (read_bits>0) flags |= (1<<28); // SPI_USR_MISO
  if (data_bits>0) flags |= (1<<27); // SPI_USR_MOSI
  spi0_setDataLengths(data_bits, read_bits);
  //Serial.printf("SPI0U:=%08x, SPI0U2:=%08x\n", flags, (7<<28)|cmd);
  SPI0U = flags;
  SPI0U2= (7<<28) | cmd;

  // copy the outcoing data to the SPI hardware
  if (data_bits>0) {
     uint32_t *src=data;
     volatile uint32_t *dst=&(SPI0W0);
     //Serial.printf("SPI0W0:=%08x\n",*src);
     for (uint32_t i=0; i<=(data_bits/sizeof(uint32_t)); i++) *dst++=*src++;
  } else {
     //Serial.printf("SPI0W0:=0\n");
     SPI0W0 = 0;
  }
  //Serial.printf("SPI0CMD:=%08x\n", SPICMDUSR);
  memw();
  SPI0CMD = SPICMDUSR;

  // wait for the command to complete
  while (SPI0CMD & SPICMDUSR) { }
  
  if (read_bits>0) {
    // copy the response back to the buffer
    //volatile uint32_t *src=&(SPI0W0);
    //uint32_t *dst=data;
    //for (uint32_t i=0; i<=(read_bits/sizeof(uint32_t)); i++) *dst++=*src++;
    *data = SPI0W0;
    //Serial.printf("SPI0W0 == %08x\n", *data);
  }
  
  SPI0U = old_spi_usr;
  SPI0U2= old_spi_usr2;
  return 0;
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

ICACHE_RAM_ATTR void flash_xmc_check() {
  if (ESP.getFlashChipVendorId() == SPI_FLASH_VENDOR_XMC) {

     #if 0
     Serial.printf("flashchip: ID:%08x, smask:%08x\n", flashchip->deviceId, flashchip->status_mask);
     int rc=SPIParamCfg(ESP.getFlashChipId(), flashchip->chip_size,
                        flashchip->block_size, flashchip->sector_size,
                        flashchip->page_size, 0x00ffffff);
     Serial.printf("flashchip: ID:%08x, smask:%08x,  rc=%d\n", flashchip->deviceId, flashchip->status_mask, rc);
     #endif
     
     //Serial.print("SelectSpiFunction()\n");
     //SelectSpiFunction();

     //Serial.print("spi_flash_attach()\n");
     //spi_flash_attach();
     
     Serial.print("Read SR3\n");
     uint32_t SR, SR1,SR2,SR3, newSR3;
     if (SPI_read_status(flashchip, &SR) != SPI_FLASH_RESULT_OK) {
        Serial.print("SPI_read_status error\n");
     }
     Serial.printf("SPI_read_status SR=%08x\n", SR);
     
     if (spi0_command(SPI_FLASH_RSR1, &SR1, 0, 8)) {
        Serial.printf("spi0_command(read SR1) failed\n");
     }
     if (spi0_command(SPI_FLASH_RSR2, &SR2, 0, 8)) {
        Serial.printf("spi0_command(read SR2) failed\n");
     }
     if (spi0_command(SPI_FLASH_RSR3, &SR3, 0, 8)) {
        Serial.printf("spi0_command(read SR3) failed\n");
     }
     
     Serial.printf("XMC Flash found, SR1=%02x, SR2=%02x, SR3=%02x\n", SR1,SR2,SR3);
     newSR3=SR3;
     
     // XMC chips default to 75% drive on their outpouts during read,
     // but can be switched to 100% by setting SR3:DRV0,DRV1 to 1,1.
     // Only needed if we are trying to run >26MHz.
     int ffreq = ESP.getFlashChipSpeed()/1000000;
     if (ffreq > 26) {
        newSR3 &= ~(SPI_FLASH_XMC_DRV_MASK << SPI_FLASH_XMC_DRV_S);
        newSR3 |= (SPI_FLASH_XMC_DRV_100 << SPI_FLASH_XMC_DRV_S);
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
          if (spi0_command(SPI_FLASH_WEVSR,NULL,0,0)) {
             Serial.print("spi0_command(write volatile enable) failed\n");
          }
          if (spi0_command(SPI_FLASH_WSR3,&newSR3,8,0)) {
             Serial.print("spi0_command(write SR3) failed\n");
          }
          if (spi0_command(SPI_FLASH_WRDI,NULL,0,0)) {
             Serial.print("spi0_command(write disable) failed\n");
          }
          if (spi0_command(SPI_FLASH_RSR3, &SR3, 0, 8)) {
             Serial.printf("spi0_command(re-read SR3) failed\n");
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


// Minimal version for startup code
void flash_xmc_check2() {
  if (ESP.getFlashChipVendorId() == SPI_FLASH_VENDOR_XMC) {
     uint32_t SR3, newSR3;
     if (!spi0_command(SPI_FLASH_RSR3, &SR3, 0, 8)) { // read SR3
        newSR3=SR3;
        if (ESP.getFlashChipSpeed()>26000000) { // >26Mhz?
           newSR3 &= ~(SPI_FLASH_XMC_DRV_MASK << SPI_FLASH_XMC_DRV_S);
           newSR3 |= (SPI_FLASH_XMC_DRV_100 << SPI_FLASH_XMC_DRV_S);
        }
        if (newSR3 != SR3) { // only write if changed
           if (!spi0_command(SPI_FLASH_WEVSR,NULL,0,0))  // write enable volatile SR
              spi0_command(SPI_FLASH_WSR3,&newSR3,8,0);  // write to SR3
           spi0_command(SPI_FLASH_WRDI,NULL,0,0);        // write disable - probably not needed
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

void flash_speed_test() {
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
}

#endif // FLASH_SPEEDTEST
