
#if FS_TEST

#define READ_FNAME "/config-x"
#define WRITE_FNAME "/config-x"

#define RANDOM_DATA "/randomdata"
#define RANDOM_COPY "/randomdata.new"
#define RANDOM_BUFSIZE 1024

void spiffs_test() {
  Serial.println("Configuring SPIFFS");
  SPIFFSConfig cfg;
  cfg.setAutoFormat(false);
  SPIFFS.setConfig(cfg);
  Serial.print("Mounting FS\n");
  SPIFFS.begin();
  
  for (int i=0; i<3; i++) {
    Serial.print("Testing write to ");
    Serial.println(WRITE_FNAME);
    File fh = SPIFFS.open(WRITE_FNAME,"w+");
    
    fh.println("blah blah blah");
    fh.printf("pass %d",i);
    fh.println("more text n stuff");
    
    fh.close();

    Serial.print("Reading ");
    Serial.println(READ_FNAME);
    fh = SPIFFS.open(READ_FNAME,"r");
    if (fh) {
       while (fh.available()) {
          String s=fh.readStringUntil('\n');
          Serial.println(s);
       }
    Serial.println("Close fh");
    fh.close();
    }
  }

  // Copy a largish test file (100K)
  if (SPIFFS.exists(RANDOM_DATA)) {
     Serial.print("Copying random test data\n");
     unsigned char *buf = (unsigned char *)malloc(RANDOM_BUFSIZE);
     unsigned long tstart=millis();
     unsigned long total=0;
     int i;
     
     for (i = 0; i<10; i++) {
         Serial.printf("  Copy %d\n", i);
         File in = SPIFFS.open(RANDOM_DATA,"r");
         File out= SPIFFS.open(RANDOM_COPY,"w+");
         long remain = in.size();
         long bs, br, bw;
         while (in.available()) {
            bs = (remain>RANDOM_BUFSIZE) ? RANDOM_BUFSIZE : remain;
            br = in.read(buf, bs);
            if (br < bs) Serial.write("ERROR: short read\n");
            bw = out.write(buf, br);
            if (bw < br) Serial.write("ERROR: short write\n");
            remain -= br;
            total += br;
         }
         in.close();
         out.close();
     }

     unsigned long tend = millis();
     free(buf);
     Serial.printf("%d copies took %ldms\n", i, tend - tstart);
     Serial.printf("Total size %ld, rate=%f bytes/ms (==K/s)\n", total, (double)total/(tend-tstart));
  } else {
     Serial.printf("%s not found, skipping speed test\n", RANDOM_DATA);
  }
  
  
  Serial.print("Unmounting FS\n");
  SPIFFS.end();
  Serial.println("SPIFFS test complete");
}

#endif // FS_TEST
