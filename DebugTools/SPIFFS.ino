
#if FS_TEST

#define READ_FNAME "/config-x"
#define WRITE_FNAME "/config-x"

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
  Serial.print("Unmounting FS\n");
  SPIFFS.end();
  Serial.println("SPIFFS test complete");
}

#endif // FS_TEST
