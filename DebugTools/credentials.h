
/*
 * SSID and password for your WiFi
 */
#define STA_SSID "<your-ssid-here>"
#define STA_PSK  "<your-pasword-here>"


/* Note the use of commas here, not periods as these macros are passed to IPAddress
 * as the parameters.  Eg  IPAddress(FIXED_IP_ADDR).
 * They are only used when NO_DHCP is set to 1 in config.h
 */
#define FIXED_IP_ADDR 192,168,1,202
#define GATEWAY_IP_ADDR 192,168,1,1
#define SUBNET_MASK 255,255,255,0

