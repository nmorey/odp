#ifndef __TEST_HELPER__
#define __TEST_HELPER__

#include <stdio.h>
#include <mppa_power.h>

#define test_assert_ret(EX)	if (!(EX)) {test_assert_print(#EX, __FILE__, __LINE__); return 1;}

extern int __mppa_power_base_exit_return_status;

static inline void test_assert_print (const char *msg, const char *file, int line)
{
	fprintf(stderr, "Assertion %s failed, file %s, line %d\n", msg, file, line);
	mppa_power_base_exit(1);
	exit(1);
}

#endif
