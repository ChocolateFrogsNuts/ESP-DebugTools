
#include "config.h"

#if FLASH_SPEEDTEST

#include <SPI.h>

#include "user_interface.h"

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

#if 0
ICACHE_RAM_ATTR 
void inline spi0_setDataLengths(uint8_t mosi_bits, uint8_t miso_bits) {
  #ifdef ESP32
    // placeholder to remind someone to fix this if they port it to ESP32
    #error ESP32 not yet supported
    // *(base+0x28) = (mosi_bits>0 ? mosi_bits-1 : 0);
    // *(base+0x2c) = (miso_bits>0 ? miso_bits-1 : 0);
  #else
    if (mosi_bits!=0) mosi_bits--;
    if (miso_bits!=0) miso_bits--;
    #ifdef SPI0_COMMAND_FULLVER
      mosi_bits &= SPIMMOSI;
      miso_bits &= SPIMMISO;
    #endif
    SPI0U1 = (miso_bits << SPILMISO) | (mosi_bits << SPILMOSI);
  #endif
}
#endif

#if 0 // MY_PRECACHE
/* precache()
 *  pre-loads flash data into the flash cache
 *  if f==0, preloads instructions starting at the address we were called from.
 *  otherwise preloads flash at the given address.
 *  All preloads are word aligned.
 */
void precache(void *f, uint32_t bytes) {
  // Size of a cache page in words. We only need to read one word per
  // page (ie 1 word in 8) for this to work.
  #define CACHE_PAGE_SIZE (32/4)
  
  register uint32_t a0 asm("a0");
  volatile uint32_t *p = (uint32_t*)((f ? (uint32_t)f : a0) & ~0x03);
  uint32_t x;
  for (uint32_t i=0; i<=(bytes/4); i+=CACHE_PAGE_SIZE, p+=CACHE_PAGE_SIZE) x=*p;
  (void)x;
}

#define PRECACHE_ATTR __attribute__((optimize("no-reorder-blocks")))

#define PRECACHE_START(tag) \
    precache(NULL,(uint8_t *)&&_precache_end_##tag - (uint8_t*)&&_precache_start_##tag); \
    _precache_start_##tag:

#define PRECACHE_END(tag) \
    _precache_end_##tag:

#else
#include "core_esp8266_features.h"
#endif // MY_PRECACHE

#if 1 // MY_SPI0_COMMAND
static inline uint32_t calc_u1(uint32_t mosi_bits, uint32_t miso_bits) {
 if (mosi_bits!=0) mosi_bits--;
 if (miso_bits!=0) miso_bits--;
 return ((miso_bits&SPIMMISO) << SPILMISO) | ((mosi_bits&SPIMMOSI) << SPILMOSI);
}

#define SPI_RESULT_OK  0
#define SPI_RESULT_ERR 1
#define SPI_RESULT_TIMEOUT 2

/*
 * critical part of spi0_command.
 * Approx 196 bytes for cut down (32bit max) version.
 *        212 bytes for full (512bit max) version
 * Kept in a separate function to prevent compiler spreading the code around too much.
 * PRECACHE_* saves having to make the function IRAM_ATTR.
 */
int PRECACHE_ATTR
_spi_command(volatile uint32_t spiIfNum,
             uint32_t spic,uint32_t spiu,uint32_t spiu1,uint32_t spiu2,
             uint32_t *data,uint32_t write_words,uint32_t read_words)
{ 
  if (spiIfNum>1) return SPI_RESULT_ERR;

  // force SPI register access via base+offest. 
  // Prevents loading individual address constants from flash.
  uint32_t *spibase = (uint32_t*)(spiIfNum ? &(SPI1CMD) : &(SPI0CMD));
  #define SPIREG(reg) *((volatile uint32_t *)(spibase+(&(reg) - &(SPI0CMD))))

  // preload any constants and functions we need into variables
  // Everything must be volatile or the optimizer can treat them as 
  // constants, resulting in the flash reads we're trying to avoid
  void *(* volatile memcpyp)(void *,const void *, size_t) = memcpy;
  int   (* volatile Wait_SPI_Idlep)(SpiFlashChip *) = Wait_SPI_Idle;
  volatile SpiFlashChip *fchip=flashchip;
  volatile uint32_t spicmdusr=SPICMDUSR;

  if (!spiIfNum) {
     // Only need to precache when using SPI0
     PRECACHE_START();
     Wait_SPI_Idlep((SpiFlashChip *)fchip);
  }
  
  uint32_t old_spi_usr = SPIREG(SPI0U);
  uint32_t old_spi_usr2= SPIREG(SPI0U2);
  uint32_t old_spi_c   = SPIREG(SPI0C);

  //SPI0S &= ~(SPISE|SPISBE|SPISSE|SPISCD);
  SPIREG(SPI0C) = spic;
  SPIREG(SPI0U) = spiu;
  SPIREG(SPI0U1)= spiu1;
  SPIREG(SPI0U2)= spiu2;

  if (write_words>0) {
     // copy the outgoing data to the SPI hardware
     memcpyp((void*)&(SPIREG(SPI0W0)),data,write_words*4);
  }

  // Start the transfer
  SPIREG(SPI0CMD) = spicmdusr;

  // wait for the command to complete
  uint32_t timeout = 1000;
  while ((SPIREG(SPI0CMD) & spicmdusr) && timeout--) {}

  if ((read_words>0) && (timeout>0)) {
     // copy the response back to the buffer
     memcpyp(data,(void *)&(SPIREG(SPI0W0)),read_words*4);
  }

  SPIREG(SPI0U) = old_spi_usr;
  SPIREG(SPI0U2)= old_spi_usr2;
  SPIREG(SPI0C) = old_spi_c;
  
  PRECACHE_END();
  return (timeout>0 ? SPI_RESULT_OK : SPI_RESULT_TIMEOUT);
}

/*  spi0_command: send a custom SPI command.
 *  This part calculates register values and does not need to be IRAM_ATTR
 */
int spi0_command(uint8_t cmd, uint32_t *data, uint32_t data_bits, uint32_t read_bits) {
  if (data_bits>(64*8)) return SPI_RESULT_ERR;
  if (read_bits>(64*8)) return SPI_RESULT_ERR;
  
  uint32_t data_words=data_bits/32;
  uint32_t read_words=read_bits/32;
  if (data_bits % 32 != 0) data_words++;
  if (read_bits % 32 != 0) read_words++;

  uint32_t flags=SPIUCOMMAND; //SPI_USR_COMMAND
  if (read_bits>0) flags |= SPIUMISO; // SPI_USR_MISO
  if (data_bits>0) flags |= SPIUMOSI; // SPI_USR_MOSI

  uint32_t spi0u2 = ((7 & SPIMCOMMAND)<<SPILCOMMAND) | cmd;
  uint32_t spi0u1 = calc_u1(data_bits, read_bits);
  
  uint32_t spi0c = (SPI0C & ~(SPICQIO | SPICDIO | SPICQOUT | SPICDOUT | SPICAHB | SPICFASTRD))
        | (SPICRESANDRES | SPICSHARE | SPICWPR | SPIC2BSE);
  
  int rc = _spi_command(0,spi0c,flags,spi0u1,spi0u2,data,data_words,read_words);
  
  // clear any bits we did not read in the last word.
  if (rc==SPI_RESULT_OK) {
     if (read_bits % 32) {
        data[read_bits/32] &= ~(0xFFFFFFFF << (read_bits % 32));
     }
  }
  return rc;
}

#else
#include "spi_utils.h"
#define spi0_command SPI0Command
#endif // MY_SPI0_COMMAND

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
     if (spi0_command(0x03, &cfg, 24, 32)!=SPI_RESULT_OK) { // 0x03 = Read Data
        Serial.print("spi0_command: read flash failed\n");
     }
     Serial.printf("spi0_command: first 4 bytes of flash=%08x\n",cfg);

     uint32_t faddr = 16;
     uint8_t fdata[64];
     memset(fdata,0,sizeof(fdata));
     if (spi_flash_read(16, (uint32_t*)fdata, sizeof(fdata))!=SPI_FLASH_RESULT_OK) {
        Serial.print("spi0_command: read flash failed\n");
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
     if (spi0_command(0x03, (uint32_t*)fdata, 24, sizeof(fdata)*8)!=SPI_RESULT_OK) { // 0x03 = Read Data
        Serial.print("spi0_command: read flash failed\n");
     }
     Serial.printf("spi0_command: bytes %d - %d of flash:", faddr, faddr+sizeof(fdata));
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
     if (spi0_command(SPI_FLASH_RSR1, &SR1, 0, 8)!=SPI_RESULT_OK) {
        Serial.printf("spi0_command(read SR1) failed\n");
     }
     Serial.print("Read SR2\n");
     if (spi0_command(SPI_FLASH_RSR2, &SR2, 0, 8)!=SPI_RESULT_OK) {
        Serial.printf("spi0_command(read SR2) failed\n");
     }
     #endif
     
     Serial.print("Read SR3\n");
     if (spi0_command(SPI_FLASH_RSR3, &SR3, 0, 8)!=SPI_RESULT_OK) {
        Serial.printf("spi0_command(read SR3) failed\n");
     }
     
     Serial.printf("XMC Flash found, SR1=%02x, SR2=%02x, SR3=%02x\n", SR1,SR2,SR3);
     newSR3=SR3;
     
     // XMC chips default to 75% drive on their outputs during read,
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
          Serial.print("WEVSR\n");
          if (spi0_command(SPI_FLASH_WEVSR,NULL,0,0)!=SPI_RESULT_OK) {
             Serial.print("spi0_command(write volatile enable) failed\n");
          }
          Serial.print("WSR3\n");
          if (spi0_command(SPI_FLASH_WSR3,&newSR3,8,0)!=SPI_RESULT_OK) {
             Serial.print("spi0_command(write SR3) failed\n");
          }
          Serial.print("WRDI\n");
          if (spi0_command(SPI_FLASH_WRDI,NULL,0,0)!=SPI_RESULT_OK) {
             Serial.print("spi0_command(write disable) failed\n");
          }
          Serial.print("RSR3\n");
          if (spi0_command(SPI_FLASH_RSR3, &SR3, 0, 8)!=SPI_RESULT_OK) {
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


/* flash_init_quirks()
 * Do any chip-specific initialization to improve performance.
 */
void flash_init_quirks() {
  switch (ESP.getFlashChipVendorId()) {
    case SPI_FLASH_VENDOR_XMC:
         uint32_t SR3, newSR3;
         if (spi0_command(SPI_FLASH_RSR3, &SR3, 0, 8)==SPI_RESULT_OK) { // read SR3
            newSR3=SR3;
            if (ESP.getFlashChipSpeed()>26000000) { // >26Mhz?
               newSR3 &= ~(SPI_FLASH_XMC_DRV_MASK << SPI_FLASH_XMC_DRV_S);
               newSR3 |= (SPI_FLASH_XMC_DRV_100 << SPI_FLASH_XMC_DRV_S);
            }
            if (newSR3 != SR3) { // only write if changed
               if (spi0_command(SPI_FLASH_WEVSR,NULL,0,0)==SPI_RESULT_OK)  // write enable volatile SR
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
