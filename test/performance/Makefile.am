include $(top_srcdir)/test/Makefile.inc

bindir      = $(datarootdir)/tests/performance

if TARGET_IS_K1_JTAG
EXECUTABLES = odp_atomic$(EXEEXT) odp_pktio_perf$(EXEEXT)
TESTSCRIPTS = odp_scheduling_run # \
 odp_l2fwd_run
endif

COMPILE_ONLY = odp_l2fwd$(EXEEXT) \
	       odp_scheduling$(EXEEXT)

if test_perf
TESTS = $(EXECUTABLES) $(TESTSCRIPTS)
endif

bin_PROGRAMS = $(EXECUTABLES) $(COMPILE_ONLY)

odp_atomic_LDFLAGS = $(AM_LDFLAGS) -static
odp_atomic_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/test
odp_scheduling_LDFLAGS = $(AM_LDFLAGS) -static
odp_scheduling_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/test

noinst_HEADERS = \
		  $(top_srcdir)/test/test_debug.h

dist_odp_atomic_SOURCES = odp_atomic.c
dist_odp_scheduling_SOURCES = odp_scheduling.c
dist_odp_pktio_perf_SOURCES = odp_pktio_perf.c
