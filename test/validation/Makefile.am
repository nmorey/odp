include $(top_srcdir)/test/Makefile.inc

bindir      = $(datarootdir)/tests/validation/

AM_CFLAGS += -I$(srcdir)/common
AM_LDFLAGS += -static

ODP_CU_COMMON=common/odp_cunit_common.c

odp_crypto_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/crypto

ODP_MODULES = buffer \
	      classification \
	      cpumask \
	      errno \
	      init \
	      queue \
	      packet \
	      pktio \
	      pool \
	      scheduler \
	      synchronizers \
	      thread \
	      time \
	      timer \
	      shmem \
	      system

if crypto
  ODP_MODULES  += crypto random
endif


SUBDIRS = common $(ODP_MODULES)

