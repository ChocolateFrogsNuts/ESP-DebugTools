
#if WITH_MEMTEST

#ifndef __MEMTEST_H__
#define __MEMTEST_H__

// Default for max memory failure results.
#define MEMTEST_FAIL_MAX 10

struct memtest_failentry {
   uint32_t *addr;
   uint32_t data;
   uint32_t count;
};

struct memtest_state {
   uint32_t *start;
   uint32_t len;
   uint32_t pass;
   uint32_t max_fails;
   uint32_t num_fails;
   uint32_t check;

   struct memtest_failentry fails[MEMTEST_FAIL_MAX];
};


extern memtest_state *memtest_init(uint32_t *start, uint32_t len, uint32_t maxfail);
extern uint32_t *memtest(struct memtest_state *info);

int print_memtest_results(struct memtest_state *info);

#endif /* __MEMTEST_H__ */

#endif /* WITH_MEMTEST */
