
#if FS_TEST

#define READ_FNAME "/config-x"
#define WRITE_FNAME "/config-x"

#define RANDOM_DATA "/randomdata"
#define RANDOM_COPY "/randomdata.new"
#define RANDOM_BUFSIZE 8192

void spiffs_speed_test(bool with_write) {
  // Copy a largish test file (100K)
  if (SPIFFS.exists(RANDOM_DATA)) {
     if (with_write)
        Serial.print("Copying random test data\n");
     else
        Serial.print("Reading random test data\n");
        
     unsigned char *buf = (unsigned char *)malloc(RANDOM_BUFSIZE);
     unsigned long tstart=millis();
     unsigned long total=0;
     int i;
     
     for (i = 0; i<10; i++) {
         Serial.printf("  Pass %d\n", i);
         File in, out;
         in = SPIFFS.open(RANDOM_DATA,"r");
         if (with_write) out= SPIFFS.open(RANDOM_COPY,"w+");
         long remain = in.size();
         long bs, br, bw;
         while (in.available()) {
            bs = (remain>RANDOM_BUFSIZE) ? RANDOM_BUFSIZE : remain;
            br = in.read(buf, bs);
            if (br < bs) Serial.write("ERROR: short read\n");
            if (with_write) {
               bw = out.write(buf, br);
               if (bw < br) Serial.write("ERROR: short write\n");
            }
            remain -= br;
            total += br;
         }
         in.close();
         if (with_write) out.close();
     }

     unsigned long tend = millis();
     free(buf);
     if (with_write)
        Serial.printf("%d copies took %ldms\n", i, tend - tstart);
     else
        Serial.printf("%d passes took %ldms\n", i, tend - tstart);
     Serial.printf("Total size %ld, rate=%f bytes/ms (==K/s)\n", total, (double)total/(tend-tstart));
  } else {
     Serial.printf("%s not found, skipping speed test\n", RANDOM_DATA);
  }  
}

void spiffs_data_test() {
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
}

  
void spiffs_test() {
  Serial.println("Configuring SPIFFS");
  SPIFFSConfig cfg;
  cfg.setAutoFormat(false);
  SPIFFS.setConfig(cfg);
  Serial.print("Mounting FS\n");
  SPIFFS.begin();

  //spiffs_data_test();
  spiffs_speed_test(false); // read-only
  //spiffs_speed_test(true);  // read-write
  
  Serial.print("Unmounting FS\n");
  SPIFFS.end();
  Serial.println("SPIFFS test complete");
}

#endif // FS_TEST
