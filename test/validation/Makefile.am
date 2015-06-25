PRE_LDADD = $(top_builddir)/test/validation/common/libcunit_common_as_main.a
include $(top_srcdir)/test/Makefile.inc

bindir      = $(datarootdir)/tests/validation/
AM_CFLAGS  += -I$(srcdir)/common
AM_LDFLAGS += -static

EXECUTABLES =  odp_buffer$(EXEEXT) \
	      odp_classification$(EXEEXT) \
	      odp_cpumask$(EXEEXT) \
	      odp_init$(EXEEXT) \
	      odp_init_abort$(EXEEXT) \
	      odp_init_log$(EXEEXT) \
	      odp_packet$(EXEEXT) \
	      odp_pool$(EXEEXT) \
	      odp_queue$(EXEEXT) \
	      odp_scheduler$(EXEEXT) \
	      odp_shared_memory$(EXEEXT) \
	      odp_synchronizers$(EXEEXT) \
	      odp_time$(EXEEXT) \
	      odp_timer$(EXEEXT) \
	      odp_thread$(EXEEXT) \
	      odp_ver_abt_log_dbg$(EXEEXT)

COMPILE_ONLY =

if crypto
  EXECUTABLES  += odp_crypto$(EXEEXT)
  COMPILE_ONLY += odp_random$(EXEEXT)
endif



if test_vald
TESTS = $(EXECUTABLES)
endif

bin_PROGRAMS = $(EXECUTABLES) $(COMPILE_ONLY)

ODP_CU_COMMON=common/odp_cunit_common.c

odp_buffer_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/buffer
dist_odp_buffer_SOURCES = odp_buffer.c $(ODP_CU_COMMON)
odp_classification_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/classification
dist_odp_classification_SOURCES = classification/odp_classification_tests.c \
				classification/odp_classification_basic.c \
				odp_classification.c $(ODP_CU_COMMON)
odp_crypto_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/crypto
dist_odp_crypto_SOURCES = crypto/odp_crypto_test_inp.c \
			  odp_crypto.c $(ODP_CU_COMMON)

dist_odp_init_SOURCES	= init/odp_init.c $(ODP_CU_COMMON)
dist_odp_init_abort_SOURCES = init/odp_init_abort.c $(ODP_CU_COMMON)
dist_odp_init_log_SOURCES = init/odp_init_log.c $(ODP_CU_COMMON)
dist_odp_queue_SOURCES	= odp_queue.c $(ODP_CU_COMMON)
dist_odp_random_SOURCES = odp_random.c $(ODP_CU_COMMON)
dist_odp_scheduler_SOURCES = odp_scheduler.c $(ODP_CU_COMMON)
dist_odp_shared_memory_SOURCES	= odp_shared_memory.c $(ODP_CU_COMMON)
dist_odp_synchronizers_SOURCES = odp_synchronizers.c $(ODP_CU_COMMON)
dist_odp_time_SOURCES   = odp_time.c $(ODP_CU_COMMON)
dist_odp_timer_SOURCES  = odp_timer.c $(ODP_CU_COMMON)
dist_odp_packet_SOURCES = odp_packet.c $(ODP_CU_COMMON)
dist_odp_pool_SOURCES = odp_pool.c $(ODP_CU_COMMON)
dist_odp_cpumask_SOURCES = odp_cpumask.c $(ODP_CU_COMMON)
dist_odp_thread_SOURCES = odp_thread.c $(ODP_CU_COMMON)
odp_ver_abt_log_dbg_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/ver_abt_log_dbg
dist_odp_ver_abt_log_dbg_SOURCES  = ver_abt_log_dbg/odp_system.c \
										 ver_abt_log_dbg/odp_errno.c \
										 ver_abt_log_dbg/odp_ver_abt_log_dbg.c $(ODP_CU_COMMON)

ODP_MODULES = pktio
SUBDIRS = common $(ODP_MODULES)

