include $(top_srcdir)/test/Makefile.inc

AM_CFLAGS += -I$(srcdir)/common
AM_LDFLAGS += -static

EXECUTABLES =  odp_buffer.kelf \
	      odp_classification.kelf \
	      odp_cpumask.kelf \
	      odp_init.kelf \
	      odp_init_abort.kelf \
	      odp_init_log.kelf \
	      odp_packet.kelf \
	      odp_pool.kelf \
	      odp_queue.kelf \
	      odp_scheduler.kelf \
	      odp_shared_memory.kelf \
	      odp_synchronizers.kelf \
	      odp_time.kelf \
	      odp_timer.kelf \
	      odp_thread.kelf \
	      odp_ver_abt_log_dbg.kelf

TESTSCRIPTS = odp_pktio_run

COMPILE_ONLY = odp_pktio.kelf

if test_vald
TESTS = $(EXECUTABLES) $(TESTSCRIPTS)
endif

dist_bin_SCRIPTS = odp_pktio_run

bin_PROGRAMS = $(EXECUTABLES) $(COMPILE_ONLY)

ODP_CU_COMMON=common/odp_cunit_common.c

odp_buffer_kelf_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/buffer
dist_odp_buffer_kelf_SOURCES = odp_buffer.c $(ODP_CU_COMMON)
odp_classification_kelf_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/classification
dist_odp_classification_kelf_SOURCES = classification/odp_classification_tests.c \
				classification/odp_classification_basic.c \
				odp_classification.c $(ODP_CU_COMMON)
#odp_crypto_kelf_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/crypto
#dist_odp_crypto_kelf_SOURCES = crypto/odp_crypto_test_inp.c \
			  odp_crypto.c $(ODP_CU_COMMON)

dist_odp_init_kelf_SOURCES	= odp_init.c
dist_odp_init_abort_kelf_SOURCES = odp_init_abort.c
dist_odp_init_log_kelf_SOURCES = odp_init_log.c
dist_odp_queue_kelf_SOURCES	= odp_queue.c $(ODP_CU_COMMON)
#dist_odp_random_kelf_SOURCES = odp_random.c $(ODP_CU_COMMON)
dist_odp_scheduler_kelf_SOURCES = odp_scheduler.c $(ODP_CU_COMMON)
dist_odp_shared_memory_kelf_SOURCES	= odp_shared_memory.c $(ODP_CU_COMMON)
dist_odp_synchronizers_kelf_SOURCES = odp_synchronizers.c $(ODP_CU_COMMON)
dist_odp_time_kelf_SOURCES   = odp_time.c $(ODP_CU_COMMON)
dist_odp_timer_kelf_SOURCES  = odp_timer.c $(ODP_CU_COMMON)
dist_odp_pktio_kelf_SOURCES	= odp_pktio.c $(ODP_CU_COMMON)
dist_odp_packet_kelf_SOURCES = odp_packet.c $(ODP_CU_COMMON)
dist_odp_pool_kelf_SOURCES = odp_pool.c $(ODP_CU_COMMON)
dist_odp_cpumask_kelf_SOURCES = odp_cpumask.c $(ODP_CU_COMMON)
dist_odp_thread_kelf_SOURCES = odp_thread.c $(ODP_CU_COMMON)
odp_ver_abt_log_dbg_kelf_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/ver_abt_log_dbg
dist_odp_ver_abt_log_dbg_kelf_SOURCES  = ver_abt_log_dbg/odp_system.c \
										 ver_abt_log_dbg/odp_errno.c \
										 ver_abt_log_dbg/odp_ver_abt_log_dbg.c $(ODP_CU_COMMON)
