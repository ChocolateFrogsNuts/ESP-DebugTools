#include "config.h"

#if STATE_CAPTURE_US

extern "C" {
  #include "user_interface.h"
  #include "cont.h"
  #include "StackThunk.h"

  typedef enum {
    FRC1_SOURCE = 0,
    NMI_SOURCE = 1,
  } FRC1_TIMER_SOURCE_TYPE;

  void hw_timer_init (FRC1_TIMER_SOURCE_TYPE source_type, u8 req);
  void hw_timer_set_func (void (* user_hw_timer_cb_set)(void) );
  void hw_timer_arm (uint32 val);
}

#define STACK_END 0x3FFFFFF0

#define F_MASK_CTX 0x07
#define F_CTX_SYS  0x00
#define F_CTX_CONT 0x01
#define F_CTX_BEAR 0x02

#define F_MASK_MAGIC   0xFF00FF00
#define F_MAGIC_USED   0xa5005a00
#define F_MAGIC_UNUSED 0x5a00a500

struct state_info {
  uint32_t id;
  uint32_t sp;
  uint32_t len;
  uint32_t flags;
  uint32_t stack_end;
  
  uint32_t *stack[MAXSTACK];
};

int stateIdx=-1;
boolean stateBusy=false;
struct state_info *states[MAXSTATES];
unsigned long hw_timer_cb_counter=0;
unsigned long hw_timer_cb_busy=0;

unsigned long store_state_counter=0;
unsigned long store_state_not_ready=0;
unsigned long store_state_busy=0;
unsigned long store_state_none_free=0;
unsigned long store_state_max_needed=0;
unsigned long store_state_required=0;
unsigned long store_state_peak=0;

struct state_info *find_state(uint32_t id) {
  if (stateIdx<0) return NULL;
  for (int i=0; i<MAXSTATES; i++) {
    struct state_info *state=states[i];
    if (state && ((state->flags & F_MASK_MAGIC) == F_MAGIC_USED)) {
       if (state->id == id) return state;
    }
  }
  return NULL;  
}

struct state_info *find_free_state() {
  if (stateIdx<0) return NULL;
  for (int i=0; i<MAXSTATES; i++) {
    struct state_info *state=states[i];
    if (state && ((state->flags & F_MASK_MAGIC) == F_MAGIC_UNUSED)) {
       return state;
    }
  }
  return NULL;
}

void print_state(struct state_info *state, int idx, struct state_info *prev)
{
    unsigned int i;
    unsigned int sp_different=0xFFFF;
    
    if ((state->flags & F_MASK_MAGIC) == F_MAGIC_USED) {
       if (prev && ((prev->flags & F_MASK_MAGIC)==F_MAGIC_USED) &&
           ((prev->flags & F_MASK_CTX) == (state->flags & F_MASK_CTX)) &&
           (prev->stack_end == state->stack_end)
          ) {
          int ofs = 0; 
          int sp1 = state->len-1;
          int sp2 = prev->len-1;
          while ((sp1>0) && (sp2>0) && (state->stack[sp1] == prev->stack[sp2])) {
             ofs++;
             sp1--;
             sp2--;
          }
          sp_different = sp1;
       }
       
       Serial.printf("\nCaptured State[%d] %d of %d bytes.",
                     idx, state->len*4, state->stack_end - state->sp);
       if (state->id) Serial.printf(" id:0x%08x", state->id);
       Serial.print("\n>>>stack>>>\n\nctx: "); // for the stack decoders
       switch (state->flags & F_MASK_CTX) {
         case F_CTX_SYS:  Serial.print("sys\n"); break;
         case F_CTX_CONT: Serial.print("cont\n"); break;
         case F_CTX_BEAR: Serial.print("bearssl\n"); break;
         default:Serial.print("Invalid Stack CTX\n"); break;
       }
       Serial.printf("sp: %08x end: %08x offset: %04x length:%d\n",
           state->sp, state->stack_end, 0, state->len*4);

       for (i=0; i<MAXSTACK && i<state->len; i++) {
           if (i % 4 == 0) Serial.printf("%08x: ", state->sp+(i*4));
           Serial.printf(" %08x", (unsigned int)state->stack[i]);
           if (i % 4 == 3) {
              if (i > sp_different) Serial.print(" =");
              Serial.printf("\n");
           }
       }
       if (i % 4) Serial.print("\n");
       Serial.print("<<<stack<<<\n");
    } else if ((state->flags & F_MASK_MAGIC) != F_MAGIC_UNUSED) {
       Serial.printf("\n\n**** WARNING ****  Corrupted captured state[%d]\n\n", idx);
    }
}

void print_states()
{
  int s;
  struct state_info *prev;
  stateBusy=true;
  Serial.printf("Timer Callback executed %lu times, busy %lu times\n", 
       hw_timer_cb_counter, hw_timer_cb_busy);
  Serial.printf("Store State executed %lu times, not ready:%lu, busy:%lu, none free:%lu  peak:%lu\n",
       store_state_counter, store_state_not_ready, 
       store_state_busy, store_state_none_free,
       store_state_peak);
  if (stateIdx<0) { 
     stateBusy=false;
     return;
  }

  print_state(states[stateIdx], stateIdx, NULL);
  s=stateIdx+1;
  prev = states[stateIdx];
  while (s!=stateIdx) {
    wdt_reset();
    if (states[s]) print_state(states[s], s, prev);
    prev = states[s];
    s++;
    if (s>=MAXSTATES) s=0;
  }
  stateBusy=false;
}

void release_state(const uint32_t id) 
{
  struct state_info *state = find_state(id);
  if (state) {
     state->flags = F_MAGIC_UNUSED;
  }
  store_state_required--;
}

void reassign_state(const uint32_t oldid, const uint32_t newid)
{
  struct state_info *state = find_state(oldid);
  if (state) {
     state->id = newid;
  }
}

void store_state(uint32_t id) 
{
#if STATE_CAPTURE_US==1
  int sp=0;
  store_state_counter++;
  store_state_required++;
  if (store_state_required > store_state_peak)
     store_state_peak=store_state_required;
     
  if (stateIdx<0) {
     store_state_not_ready++;
  } else if (stateBusy) {
     store_state_busy++;
  } else {
     struct state_info *state = find_free_state();
     if (state) {
        stateBusy=true;
        _store_state(state, (uint32_t)&sp, id);
        stateBusy=false;
     } else {
        store_state_none_free++;
     }
  }
#else
  (void)id;
#endif
}

void _store_state(struct state_info *state, const uint32_t sp, const uint32_t id) 
{
  uint32_t cont_stack_start = (uint32_t) &(g_pcont->stack);
  uint32_t cont_stack_end = (uint32_t) g_pcont->stack_end;
  uint32_t stack_end;

  if (sp > stack_thunk_get_stack_bot() && sp <= stack_thunk_get_stack_top()) {
       stack_end = stack_thunk_get_stack_top();
       state->flags = F_MAGIC_USED | F_CTX_BEAR;
    } else if (sp > cont_stack_start && sp<cont_stack_end) {
       stack_end = cont_stack_end;
       state->flags = F_MAGIC_USED | F_CTX_CONT;
    } else {
       stack_end = STACK_END;
       state->flags = F_MAGIC_USED | F_CTX_SYS;
    }
  
  uint32_t len = (stack_end - sp)/4;
  if (len>=MAXSTACK) len=MAXSTACK-1;
  state->id = id;
  state->sp = sp;
  state->len= len;
  state->stack_end = stack_end;
  memcpy((void *)state->stack, (void *)sp, len*4);
}


extern "C" {
  void hw_timer_cb(void)
  {
    // essential regs will already be stored somewhere (stack?)
    // Just need to locate and print them in print_states()
    register uint32_t sp asm("a1");
    int saved = xt_rsil(15);
    hw_timer_cb_counter++;
    if (stateBusy || (stateIdx<0) || (states[stateIdx]==NULL)) {
       hw_timer_cb_busy++;
    } else {
      stateBusy=true;
      _store_state(states[stateIdx], sp, 0);
      stateIdx++;
      if (stateIdx>=MAXSTATES) stateIdx=0;
      stateBusy=false;
    }
    xt_wsr_ps(saved);
  }
}

void done_states()
{
  if (stateIdx<0) return;
  stateIdx=-1;
  for (int i=0; i<MAXSTATES; i++) {
    if (states[i]) {
      states[i]->flags = 0;
      free(states[i]);
      states[i]=NULL;
    }
  }
}

void ICACHE_FLASH_ATTR init_states()
{
  if (stateIdx>=0) {
     // state storage already initialized
     return;
  }
  
  for (int i=0; i<MAXSTATES; i++) {
    states[i]=(struct state_info *)malloc(sizeof(struct state_info));
    if (states[i]) {
       memset(states[i], 0, sizeof(struct state_info));
       states[i]->flags = F_MAGIC_UNUSED;
    }
  }
  stateIdx=0;
  stateBusy=false;
  hw_timer_cb_counter=0;
  hw_timer_cb_busy=0;
  store_state_counter=0;
  store_state_busy=0;
  store_state_none_free=0;
  store_state_required=0;
  store_state_peak=0;
  
  #if STATE_CAPTURE_US>100
     hw_timer_init(FRC1_SOURCE, 1);
     hw_timer_set_func(hw_timer_cb);
     hw_timer_arm(STATE_CAPTURE_US); // us
  #endif
}

#endif // STATE_CAPTURE_US
