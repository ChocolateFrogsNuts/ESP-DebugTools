#include "config.h"

#if INTR_LOCK_TEST

// NOTE: this must match ETS_INTR_LOCK_NEST_MAX in cores/esp8266/core_esp8266_main.cpp
#define ETS_INTR_LOCK_NEST_MAX 7

#define MAX_CALLERS 100

int  ets_intr_lock_maxnest=0;
int  ets_intr_lock_minnest=0;
int  ets_intr_lock_curnest=0;

struct caller_info {
  uint32_t addr;
  uint32_t count;
  int      minnest, maxnest;
};
struct caller_stats {
  uint32_t num;
  struct caller_info calls[MAX_CALLERS];
};

struct caller_stats lock_stats = { .num=0, .calls={ {0,0,0,0} } };
struct caller_stats unlock_stats = { .num=0, .calls={ {0,0,0,0} } };

extern uint16_t ets_intr_lock_stack[ETS_INTR_LOCK_NEST_MAX];
extern byte     ets_intr_lock_stack_ptr;

void record_call(struct caller_stats *s, uint32_t addr, byte nest) {
  uint32_t i=0;
  while ((i<s->num) && (addr!=s->calls[i].addr)) i++;
  if (i<s->num) {
     s->calls[i].count++;
     if (s->calls[i].minnest > nest)
        s->calls[i].minnest = nest;
     if (s->calls[i].maxnest < nest)
        s->calls[i].maxnest = nest;
     return;
  }
  if (i<MAX_CALLERS) {
     s->calls[s->num].addr = addr;
     s->calls[s->num].count= 1;
     s->calls[s->num].minnest = nest;
     s->calls[s->num].maxnest = nest;
     s->num++;
  }
}

void ets_intr_lock() {
  register uint32_t a0 asm("a0");
  uint32_t caller=a0;
  uint32_t ps;
  
  ets_intr_lock_curnest++;
  if (ets_intr_lock_curnest>ets_intr_lock_maxnest) ets_intr_lock_maxnest=ets_intr_lock_curnest;
  
  if (ets_intr_lock_stack_ptr < ETS_INTR_LOCK_NEST_MAX) {
     ps = xt_rsil(3);
     ets_intr_lock_stack[ets_intr_lock_stack_ptr++]=ps;
  } else {
     ps=xt_rsil(3);
     // overflow - complain.
  }
  // *((uint32_t *)(0x3fffdcc0)) = ps; // why? what uses this?
  record_call(&lock_stats, caller, ets_intr_lock_curnest);
}

void ets_intr_unlock() {
  register uint32_t a0 asm("a0");
  uint32_t caller=a0;
  record_call(&unlock_stats, caller, ets_intr_lock_curnest);

  ets_intr_lock_curnest--;
  if (ets_intr_lock_curnest<ets_intr_lock_minnest) ets_intr_lock_minnest=ets_intr_lock_curnest;

  if (ets_intr_lock_stack_ptr > 0) {
     register uint32_t ps=ets_intr_lock_stack[--ets_intr_lock_stack_ptr];
     xt_wsr_ps(ps);
  }
  else { 
     xt_rsil(0); // underflow - somebody is a bad coder
  }
}


void print_lock_stats(struct caller_stats *s) {
  uint32_t i=0;
  Serial.printf("%d callers\n", s->num);
  while (i<s->num) {
      Serial.printf("   0x%08x %8d %2d %2d\n", 
           s->calls[i].addr, s->calls[i].count, 
           s->calls[i].minnest, s->calls[i].maxnest);
      i++;
  }
}

void print_intr_lock_stats() {
     Serial.printf("ets_intr_lock_minnest=%d, ets_intr_lock_maxnest=%d\n", 
         ets_intr_lock_minnest, ets_intr_lock_maxnest);

     Serial.printf("ets_intr_lock_nest stats\n");
     print_lock_stats(&lock_stats);
     Serial.printf("ets_intr_unlock_nest stats\n");
     print_lock_stats(&unlock_stats);
}

#endif // INTR_LOCK_TEST

int wakeup_enable_a=0;
int wakeup_enable_b=0;
int ets_post_a=0;
int ets_post_b=0;
int ets_post_c=0;
int ets_post_d=0;
int glue_ets_post=0;

void print_test_counters() {
  Serial.printf("func: interrupts disabled, interrupts enabled\n");
  Serial.printf("gpio_pin_wakeup_enable : %d, %d\n", wakeup_enable_a, wakeup_enable_b);
  Serial.printf("ets_post               : %d, %d\n", ets_post_a, ets_post_b);
  Serial.printf("ets_post (glue)        : %d, %d\n", ets_post_c, ets_post_d);
  print_states();
}

bool ets_post_stub(uint8 prio, ETSSignal sig, ETSParam par) {
  uint32_t saved;
  asm volatile ("rsr %0,ps":"=a" (saved));

  if (glue_ets_post) {
     if (saved & 0x0F) ets_post_c++; else ets_post_d++;
  } else {
     if (saved & 0x0F) ets_post_a++; else ets_post_b++;
     //if (saved & 0x0F) store_state(0);
  }
  bool rc = ets_post(prio, sig, par);
  //xt_wsr_ps(saved);
  return rc;
}

#if 0
void gpio_pin_wakeup_enable_stub(uint32 i, GPIO_INT_TYPE intr_state) {
  uint32_t saved;
  asm volatile ("rsr %0,ps":"=a" (saved));

  if (saved & 0x0F) wakeup_enable_a++; else wakeup_enable_b++;
  gpio_pin_wakeup_enable(i, intr_state);
  xt_wsr_ps(saved);
}
#endif
