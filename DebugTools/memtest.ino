#include "config.h"

#if WITH_MEMTEST

#include "memtest.h"

// This is only needed for storing the results of any memory tests in early
// startup - such as a heap test before creating the heap - so that they can
// be displayed once we get up and running.
// We can't just allocate it on the heap because we don't have a heap yet,
// and we can't put it on the stack because we need it afterwards to display
// results. We *could* put it on the stack and move it to heap later, but
// as a debugging tool that's not worth the trouble.
struct memtest_state memtest_global_results;


// returns a 32-bit xor of the used bytes in *info
// Could be replaced by a crc32 func.
static uint32_t xor_check(struct memtest_state *info) {
  uint32_t len = (char *)&info->check - (char*)info + sizeof(info->check);
  uint32_t *p=(uint32_t *)info;
  uint32_t c=*p++;
  len = len/sizeof(uint32_t) - 1;
  while (len--) c ^= *p++;

  len=info->num_fails * sizeof(info->fails[0]) / sizeof(uint32_t);
  p=(uint32_t*)info->fails;
  while (len--) c ^= *p++;

  return c;
}

/* malloc and initialize a memtest state.
 * start    :  start address
 * len      :  length in bytes to test
 * maxfail  :  max nubmer of failed addresses to record.
 *
 * maxfail==0 is a special case and initializes/returns the global memtest_state.
 */
struct memtest_state *
memtest_init(uint32_t *start, uint32_t len, uint32_t maxfail)
{
  struct memtest_state *info;
  if (maxfail==0) {
     info=&memtest_global_results;
     info->max_fails = MEMTEST_FAIL_MAX;
  } else {
     info = (struct memtest_state*)malloc(sizeof(struct memtest_state) +
              sizeof(struct memtest_failentry) * (maxfail-MEMTEST_FAIL_MAX));
     if (!info) return NULL;
     info->max_fails = maxfail;
  }

  info->start = start;
  info->len   = len;
  info->pass  = 0;
  info->num_fails=0;
  info->check = 0;

  // clear the fail list
#if 1
  memset(info->fails, 0, info->max_fails * sizeof(info->fails[0]));
#else
  for (unsigned int i=0; i<info->max_fails; i++) {
      info->fails[i].addr=NULL;
      info->fails[i].data=0;
      info->fails[i].count=0;
      }
#endif

  info->check = xor_check(info);
  return info;
}

/*
 * Rigourous memory test with alternating patterns and a validated
 * status structure.  Only validates multiples of PATTERN_GROUP_SIZE words.
 * Any extra bytes at the end will not get checked, rather than writing
 * code that is likely to never get used in this environment.
 * Leaves the memory with whatever patterns were last used.
 * Takes approx 22ms per 48KB of ram tested.
 * Paramters:
 *         info - a state structure created by memtest_init()
 *         
 * Return: NULL - no problems,
 *         info parameter - if the state failed the integrity check or
 *         the address of the first failing word.
 */
uint32_t* memtest(struct memtest_state *info) {

  // patterns are used in groups. Array must be a multiple of GROUP_SIZE
  // This pattern list is by no means perfect or exhaustive, but it's 
  // decent while being fairly quick to run.
  #define PATTERN_GROUP_SIZE 2
  uint32_t patterns[]={0xAAAAAAAA,0x55555555,
                       0x55555555,0xAAAAAAAA,
                       0x5A5A5A5A,0xA5A5A5A5,
                       0xA5A5A5A5,0x5A5A5A5A,
                       0xFFFFFFFF,0x00000000,
                       0x00000000,0xFFFFFFFF};
  uint32_t *p;
  uint32_t *firstfail=NULL;
  uint32_t saved_ps;
  
  uint32_t len;
  unsigned int group;
  unsigned int j;
  unsigned int k;

  // validate the info struct and increment the pass counter.
  if (xor_check(info)) {
     return (uint32_t *)info;
  }
  info->pass++;
  info->check=0;
  info->check=xor_check(info);
  len = info->len / sizeof(patterns[0]);

  saved_ps = xt_rsil(3);
  group=0;
  while (group < (sizeof(patterns)/sizeof(patterns[0])) ) {

     // write this group of patterns, repeating
     p=info->start;
     j=0;
     while (j<len) {
        for (k=0; k<PATTERN_GROUP_SIZE; k++)
            *p++=patterns[group+k];
        j+=PATTERN_GROUP_SIZE;
     }

     // read back
     p=info->start;
     j=0;
     while (j<len) {
        for (k=0; k<PATTERN_GROUP_SIZE; k++) {
            if (*p!=patterns[group+k]) {
               if (!firstfail) firstfail=p;
               if (xor_check(info)) {
                  // something has happened to the info struct
                  xt_wsr_ps(saved_ps);
                  return (uint32_t*)info;
               }
               // don't record a repeat address
               uint32_t count;
               unsigned int i=0;
               while ((i<info->num_fails) && (info->fails[i].addr!=p)) i++;

               if (i>=info->max_fails) { // not found + fail list full
                  xt_wsr_ps(saved_ps);
                  return firstfail;
                  }
               if (i==info->num_fails) { // not found, add a record
                  info->num_fails++;
                  info->fails[i].addr = p;
                  info->fails[i].data = *p;
                  info->fails[i].count= 0;
               }
               count=info->fails[i].count;
               info->fails[i].count++;

               // verify everything we just updated
               if ((info->num_fails==(i+1)) &&
                   (info->fails[i].addr==p) &&
                   (info->fails[i].data==*p) &&
                   (info->fails[i].count==(count+1))
                  ) {
                  // all good, update check.
                  info->check=0;
                  info->check=xor_check(info);
               } else {
                  return (uint32_t*)info;
               }
            }
            p++;
        }
        j+=PATTERN_GROUP_SIZE;
     }

     group += PATTERN_GROUP_SIZE; // next group of patterns
  }

  xt_wsr_ps(saved_ps);
  return firstfail;
}


int print_memtest_results(struct memtest_state *info) {
  if (info==NULL) info=&memtest_global_results;
  if (info->num_fails) Serial.print("\n\n");
  Serial.printf("memtest: start=%p, len=%d, passes=%d  ",
                info->start, info->len, info->pass);
  if (info->num_fails) {
     Serial.print("FAIL\n");
     uint32_t fp=0;
     while (fp < info->num_fails) {
       Serial.printf("             Fail at %p with 0x%08x\n", 
               info->fails[fp].addr, info->fails[fp].data);
       fp++;
     }
     Serial.printf("%d records\n", fp);
     Serial.print("\n\n");
  } else {
     Serial.print("PASS\n");
  }
  return info->num_fails;
}

#endif // WITH_MEMTEST
