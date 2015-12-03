#ifndef _MPPA_PCIE_DEBUG_H
#define _MPPA_PCIE_DEBUG_H

#include <string.h>

static inline
int no_printf(__attribute__((unused)) const char *fmt , ...)
{
	return 0;
}

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifndef NDEBUG
#    define dbg_printf(fmt, args...) \
	printf("[DBG] %s:%d: " fmt,  __FILENAME__, __LINE__, ## args)
#else
#    define dbg_printf(fmt, args...)	no_printf(fmt, ## args)
#endif

#endif
