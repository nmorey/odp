include $(top_srcdir)/test/Makefile.inc

EXECUTABLES = odp_atomic.kelf odp_pktio_perf.kelf

COMPILE_ONLY = odp_l2fwd.kelf \
	       odp_scheduling.kelf

TESTSCRIPTS = odp_l2fwd_run \
	      odp_scheduling_run

if test_perf
TESTS = $(EXECUTABLES) $(TESTSCRIPTS)
endif

bin_PROGRAMS = $(EXECUTABLES) $(COMPILE_ONLY)

odp_atomic_kelf_LDFLAGS = $(AM_LDFLAGS) -static
odp_atomic_kelf_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/test
odp_scheduling_kelf_LDFLAGS = $(AM_LDFLAGS) -static
odp_scheduling_kelf_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/test

noinst_HEADERS = \
		  $(top_srcdir)/test/test_debug.h

dist_odp_atomic_kelf_SOURCES = odp_atomic.c
dist_odp_scheduling_kelf_SOURCES = odp_scheduling.c
dist_odp_pktio_perf_kelf_SOURCES = odp_pktio_perf.c
