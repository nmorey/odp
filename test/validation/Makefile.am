include $(top_srcdir)/test/Makefile.inc

bindir      = $(datarootdir)/tests/validation/

ODP_MODULES = buffer \
	      classification \
	      config \
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
  ODP_MODULES  +=  random
endif


SUBDIRS = common $(ODP_MODULES)

