/* My SDK has been modified to have NONOSDK set to the SDK version
 * so I can select between 6 different versions of the SDK.
 * these definitions are to make this file backward compatible.
 * And yes, for the current ESP8266/Arduino that means this file is not
 * used.
 */
#ifndef NONOSDK
#ifdef NONOSDK3V0
  #define NONOSDK 300000
#else
  #ifdef NONOSDK22y
    #define NONOSDK 222100
  #else
    #define NONOSDK 221000
  #endif
#endif
#endif


/* Now the code required by newer SDK3 versions */

#if NONOSDK>=300109
extern "C" {

//#ifndef SPI_FLASH_SIZE_MAP
#define SPI_FLASH_SIZE_MAP 4
//#endif

#if ((SPI_FLASH_SIZE_MAP == 0) || (SPI_FLASH_SIZE_MAP == 1))
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 2)
#define SYSTEM_PARTITION_OTA_SIZE                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR              0xfb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR            0xfc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR    0xfd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0x7c000
#elif (SPI_FLASH_SIZE_MAP == 3)
#define SYSTEM_PARTITION_OTA_SIZE                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR              0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR            0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR    0x1fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0x7c000
#elif (SPI_FLASH_SIZE_MAP == 4)
#define SYSTEM_PARTITION_OTA_SIZE                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR              0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR            0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR    0x3fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0x7c000
#elif (SPI_FLASH_SIZE_MAP == 5)
#define SYSTEM_PARTITION_OTA_SIZE                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR               0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR              0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR            0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR    0x1fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0xfc000
#elif (SPI_FLASH_SIZE_MAP == 6)
#define SYSTEM_PARTITION_OTA_SIZE                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR               0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR              0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR            0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR    0x3fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR 0xfc000
#else
#error "The flash map is not supported"
#endif

#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM      SYSTEM_PARTITION_CUSTOMER_BEGIN

#if FOTA_MAP
static const partition_item_t at_partition_table[] = {
    { SYSTEM_PARTITION_BOOTLOADER,             0x0,                       0x1000},
    { SYSTEM_PARTITION_OTA_1,               0x1000,                       SYSTEM_PARTITION_OTA_SIZE},
    { SYSTEM_PARTITION_OTA_2,               SYSTEM_PARTITION_OTA_2_ADDR,  SYSTEM_PARTITION_OTA_SIZE},
    { SYSTEM_PARTITION_RF_CAL,              SYSTEM_PARTITION_RF_CAL_ADDR,              0x1000},
    { SYSTEM_PARTITION_PHY_DATA,            SYSTEM_PARTITION_PHY_DATA_ADDR,            0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER,    SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR,    0x3000},
    { SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM, SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR, 0x1000},
};
#else
#define EAGLE_FLASH_BIN_ADDR        (partition_type_t)(SYSTEM_PARTITION_CUSTOMER_BEGIN + 1)
#define EAGLE_IROM0TEXT_BIN_ADDR    (partition_type_t)(SYSTEM_PARTITION_CUSTOMER_BEGIN + 2)

static const partition_item_t at_partition_table[] = {
  { EAGLE_FLASH_BIN_ADDR,   0x00000, 0x10000},
  { EAGLE_IROM0TEXT_BIN_ADDR, 0x10000, 0x60000},
  { SYSTEM_PARTITION_RF_CAL, SYSTEM_PARTITION_RF_CAL_ADDR, 0x1000},
  { SYSTEM_PARTITION_PHY_DATA, SYSTEM_PARTITION_PHY_DATA_ADDR, 0x1000},
  { SYSTEM_PARTITION_SYSTEM_PARAMETER,SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR, 0x3000},
};
#endif

void ICACHE_FLASH_ATTR user_pre_init(void)
{
  // uart_init(BIT_RATE_115200, BIT_RATE_115200);

  uart_init(UART0, 115200, UART_8N1, UART_FULL, 1, 256);
  
  os_printf("\r\nRegistering partition table\r\n");
  
  if(!system_partition_table_regist(at_partition_table, sizeof(at_partition_table)/sizeof(at_partition_table[0]),SPI_FLASH_SIZE_MAP)) {
    os_printf("system_partition_table_regist fail\r\n");
    while(1);
  }
}

}
#endif // NONOSDK>=300109
