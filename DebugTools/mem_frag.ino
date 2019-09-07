#include "config.h"

#if MEM_FRAG_TEST

extern "C" {
  #include "umm_malloc/umm_malloc.h"
}

#ifdef UMM_REDEFINE_MEM_FUNCTIONS
#define UMM_FREE free
#define UMM_MALLOC malloc
#else
#define UMM_FREE umm_free
#define UMM_MALLOC umm_malloc
#endif

#define BUFFERS   50
#define BUF_FILL  0x00
#define BUF_TAINT 0xF0
#define NUM_PATTERNS 4

char *buffers[BUFFERS];
char  pattern[BUFFERS];
char  patterns[NUM_PATTERNS]={0x00, 0xFF, 0xAA, 0x55};
int   buflen[BUFFERS];
int buf=0;
int bufinc=0;

void mem_frag_release(int bufidx, bool verbose) {
     if (verbose) Serial.printf("F%d ", bufidx);
     int i=0;
     char pat = patterns[pattern[bufidx]];
     while ((i<buflen[bufidx]) && (buffers[bufidx][i]==pat)) i++;
     if (i<buflen[bufidx]) {
        Serial.printf("\nbuffer %d (%d bytes) is tainted with %02x at byte %d (should be %02x)\n", 
                     bufidx, buflen[bufidx], buffers[bufidx][i], i, pat);
        Serial.printf("Address of taint: %p\n", &buffers[bufidx][i]);
        for (int j=0; j<32; j++) {
            Serial.printf("%02x ", buffers[bufidx][i+j]);
        }
     }
     pattern[bufidx]++;
     if (pattern[bufidx]>=NUM_PATTERNS) pattern[bufidx]=0;
     
     memset(buffers[bufidx], BUF_TAINT, buflen[bufidx]);
     UMM_FREE(buffers[bufidx]);
     buffers[bufidx]=NULL;
}

void mem_frag_alloc(int bufidx, bool verbose) {
     if (verbose) Serial.printf("M%d ", bufidx);
     buflen[bufidx]=(0x80 - (bufinc & 0x7F)) * 10;
     buffers[bufidx]=(char*)UMM_MALLOC(buflen[bufidx]);
     if (buffers[bufidx]!=NULL) {
         memset(buffers[bufidx], patterns[pattern[bufidx]], buflen[bufidx]);
     } else {
         Serial.printf("M%d* ", bufidx);
     }
}

void mem_frag_buffers(bool verbose) {
  if (buffers[buf]!=NULL) {
     mem_frag_release(buf, verbose); 
  } else {
     mem_frag_alloc(buf, verbose);
  }

  buf += (bufinc & 0x01)+1;
  if (buf>=BUFFERS) buf=0;
  bufinc++;
  if (bufinc >= 0xFFFF) bufinc=0;
}

void mem_frag_init() {
  for (int i=0; i<BUFFERS; i++) {
      buffers[i]=NULL;
      pattern[i]=0;
  }
  // fragment the heap a bit
  for (int i=0; i<BUFFERS*3; i++) {
      mem_frag_buffers(false);
      if ((i%10)==0) wdt_reset();
  }
}

void mem_frag_done() {
  for (int i=0; i<BUFFERS; i++) {
      if (buffers[i]) mem_frag_release(i, false);
  }
}

#endif // MEM_FRAG_TEST
