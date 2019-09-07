#include "config.h"

#if WITH_MEMTEST

#include "memtest.h"


void do_memory_tests() {

  Serial.print("Heap Memtest Results\n");
  int heap_fails = print_memtest_results(NULL);
  
  // now check the current (context or system) stack
  test_stack();
  Serial.print("Stack Memtest Results\n");
  int stack_fails = print_memtest_results(NULL);

  if (heap_fails || stack_fails) {
     Serial.printf("Memory Test Failed\n");
     while (1) wdt_reset();
  }
}

extern "C" {
  #include "cont.h"
  #include "StackThunk.h"
}

int _test_stack(uint32_t sp) {
  uint32_t *err;
  uint32_t stack_start;
  uint32_t stack_end;
  uint32_t stacksize;
  uint32_t *stack;
  uint32_t cont_stack_start = (uint32_t) &(g_pcont->stack);
  uint32_t cont_stack_end = (uint32_t) g_pcont->stack_end;

  if (sp > stack_thunk_get_stack_bot() && sp <= stack_thunk_get_stack_top()) {
       stack_start = stack_thunk_get_stack_bot();
       stack_end   = stack_thunk_get_stack_top();
  } else if (sp>cont_stack_start && sp<cont_stack_end) {
       stack_start = cont_stack_start;
       stack_end   = cont_stack_end;
  } else {
       stack_start = 0x3FFFF000;
       stack_end   = 0x3FFFFFF0;
  }
  stack = (uint32_t *)stack_start;
  stacksize = stack_end - stack_start;
  int passes=10;

  if (((uint32_t)&stacksize > stack_start) && ((uint32_t)&stacksize < stack_end)) {
     Serial.print("  Can't test my own stack (temp stack switch failed)\n");
     return 1;
  }
  if ((sp < stack_start) || (sp > stack_end)) {
     Serial.printf("  Can't find the correct stack bounds for sp=0x%08x\n", sp);
     return 2;
  }
  
  struct memtest_state *inf = memtest_init((uint32_t*)stack_start, stacksize, 0);
  if (!inf) {
     Serial.print("  Can't obtain stack test state data\n");
     return 3;
  }

  byte *backup = (byte *)malloc(stacksize);
  if (!backup) {
     Serial.print("  Can't allocate memory for stack backup\n");
     return 4;
  }
  Serial.printf("Running %d passes on the stack from %p to 0x%08x (%d bytes) sp=0x%08x\n",
       passes, stack, stack_end, stacksize, sp);
  //delay(50);
  
  noInterrupts();
  memcpy(backup, stack, stacksize);
  for (int i=0; (i<passes) && !(err=memtest(inf)); i++) wdt_reset();
  memcpy(stack, backup, stacksize);
  interrupts();

  free(backup);
  return 0;
}

void test_stack() {
  // swap to a temporary stack and call the real test func
  register uint32_t sp asm("a1");
  uint32_t real_sp = sp;
  Serial.print("Testing stack\n");
  int tempsize = 4096;
  uint32_t *tempptr;
  byte *tempstack = (byte *)malloc(tempsize);
  int rc=9;
  if (tempstack) {
     tempptr = (uint32_t*)(tempstack + tempsize - (16*6));
     Serial.printf("Using temporary stack at %p, sp=%p\n", tempstack, tempptr);
     asm volatile (
          "mov  a2, %[real_sp]\n\t"
          "s32i a2, %[tempptr], 0\n\t"
          "mov  a1, %[tempptr]\n\t"
          "call0 _test_stack\n\t"
          "l32i a1, a1, 0\n\t"
          "mov  %[rc], a2\n\t"
          : [rc] "=r" (rc)
          : [real_sp] "r" (real_sp), [tempptr] "r" (tempptr)
          : "a2"
     );
     free(tempstack);
  } else {
     Serial.printf("Unable to allocate a temporary stack\n");
  }
}

#endif // WITH_MEMTEST
