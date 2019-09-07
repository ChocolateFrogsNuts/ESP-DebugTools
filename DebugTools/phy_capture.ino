
#include "config.h"

#if WIFI_PROMISC || PHY_CAPTURE
unsigned long pkts_in=0, pkts_out=0, bytes_in=0, bytes_out=0;
unsigned long pkts_in_failed=0, pkts_out_failed=0;

void phy_stats_update(int len, int out, int success) {
  if (out==0) {
     pkts_in++;
     bytes_in+=len;
     if (!success) pkts_in_failed++;
  } else {
     pkts_out++;
     bytes_out+=len;
     if (!success) pkts_out_failed++;
  }
}

void phy_stats() {
  Serial.print ("                In       Out\n");
  Serial.printf("Packets   %8ld  %6ld\n", pkts_in, pkts_out);
  Serial.printf("Bytes     %8ld  %6ld\n", bytes_in, bytes_out);
  Serial.printf("Failed    %8ld  %6ld\n", pkts_in_failed, pkts_out_failed);
  Serial.println();
}

void sprint_mac(char *s, const byte *mac) {
  sprintf(s,"%02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

void sprint_ip(char *s, const byte *ip) {
  sprintf(s, "%d.%d.%d.%d", ip[0],ip[1],ip[2],ip[3]);
}


void xdump(const char* what, const char* data, uint16_t len)
{
        #define N 16
        size_t i, j;
        if (what!=NULL) Serial.printf("%s: len=%d\n", what, len);
        for (i = 0; i < len; i += N)
        {
                for (j = i; j < i + N && j < len; j++)
                    Serial.printf("%02x ", data[j]);
                for (; j < i + N; j++)
                    Serial.print("   ");
                for (j = i; j < i + N && j < len; j++)
                    Serial.printf("%c", (data[j] >= 32 && data[j] < 127)? data[j]: '.');
                Serial.print("\n");
        }
}

void dump_pkt(const char *data, int len) {
  if (len >= 14) {
     char mac1[20],mac2[20];
     char ip1[20],ip2[20];
     int  proto;
     int ptype=(data[12]<<8)|data[13];
     sprint_mac(mac1,(byte*)data);
     sprint_mac(mac2,(byte*)data+6);
     
     Serial.printf("\n%s -> %s type %04x ", mac1, mac2, ptype);
     switch (ptype) {
        case 0x0800: Serial.print("IPv4");
             proto = data[23];
             sprint_ip(ip1, (byte*)data+26);
             sprint_ip(ip2, (byte*)data+30);
             Serial.printf(" %s->%s ", ip1, ip2);
             switch (proto) {
               case  1: Serial.print("ICMP"); break;
               case  6: Serial.print("TCP"); break;
               case 17: Serial.print("UDP"); break;
               default:   Serial.printf("%d", proto);
             }
             break;
        case 0x0806: Serial.print("ARP");
             proto = (data[20] << 8) | data[21];
             sprint_ip(ip1, (byte*)data+38);
             sprint_ip(ip2, (byte*)data+28);
             sprint_mac(mac1, (byte*)data+22);
             if (proto == 1) {
                Serial.printf(" who has %s tell %s", ip1, ip2);
             } else if (proto == 2) {
                Serial.printf(" %s is at %s", ip2, mac1);
             } else {
                Serial.printf(" unknown opcode %d", proto);
             }
             break;
        case 0x86dd: Serial.print("IPv6"); break;
        case 0x876B: Serial.print("TCP-Compression"); break;
        case 0x8863: Serial.print("PPPoE-Discovery"); break;
        case 0x8864: Serial.print("PPPoE-Session"); break;
        case 0x88C7: Serial.print("802.11-PreAuth"); break;
        case 0x88cc: Serial.print("802.1AB-LLDP"); break;
        case 0x88E5: Serial.print("802.1AE-MediaAccessControlSecurity"); break;
        case 0x890D: Serial.print("802.11r-FastRoamingRemoteRequest"); break;
        case 0x8929: Serial.print("802.1Qbg-Multiple-I-SID"); break;
        case 0x8940: Serial.print("802.1Qbg-ECP"); break;
        case 0x0900: Serial.print("Loopback"); break;
        default:     Serial.printf("%04x", ptype);
     }
  }
  Serial.print("\n");
}

#endif // WIFI_PROMISC || PHY_CAPTURE

#if WIFI_PROMISC

void wifi_promisc_cb(const uint8 *buf, uint16 len) {
  bool do_dump=true;
  Serial.printf("promisc: buffer %p, %d bytes", buf, len);
  if (len==128) { // assume beacon
     char ap[32];
     int  l=(buf[48]<<8)|buf[49];
     memcpy(ap,buf+50,l);
     ap[l]='\0';
     Serial.printf("  beacon %s", ap);
     do_dump=false;
  }
  else if (len==60) {
     // not sure what this is - MGMT?
     Serial.print(" MGMT?");
     do_dump=false;
  }
  else if (len==12) {
     Serial.print(" short MGMT?");
     do_dump=false;
  }
  Serial.print("\n");
  //dump_pkt((char *)buf,len);

// WIFI_PROMISC_DUMP: 0=none, 1=unknown traffic, 2=all traffic
#if WIFI_PROMISC_DUMP
#if WIFI_PROMISC_DUMP<2
  if (do_dump)
#endif
     xdump(NULL,(char *)buf,len);
#endif
}

void wifi_promisc_init(int channel) {
  wifi_set_promiscuous_rx_cb(wifi_promisc_cb);
  if (channel) wifi_set_channel(channel);
  wifi_promiscuous_enable(1);
}

#elif PHY_CAPTURE // and !WIFI_PROMISC

void phy_capture_cb(int netif_idx, const char* data, size_t len, int out, int success) {
  (void)netif_idx;
  (void)data;
#if 0
  if (1 || out) {
     Serial.print("\nNETWORK: IF=");
     Serial.print(netif_idx);
     Serial.print(" buffer=");
     Serial.printf("%p",data);
     Serial.print(" len=");
     Serial.print(len);
     Serial.print( out ? " out " : " in " );
     Serial.print( success ? "OK" : "ERR" );
  }
#endif

#if PHY_CAPTURE_DUMP
  dump_pkt(data, len);
#endif // PHY_CAPTURE_DUMP

  phy_stats_update(len,out,success);
}

void phy_capture_init() {
    phy_capture = phy_capture_cb;
}

#endif // PHY_CAPTURE
