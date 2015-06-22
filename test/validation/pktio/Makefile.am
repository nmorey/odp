include ../Makefile.inc

noinst_LIBRARIES = libpktio.a
libpktio_a_SOURCES = pktio.c

bin_PROGRAMS = pktio_main.kelf
dist_pktio_main_kelf_SOURCES = pktio_main.c
pktio_main_kelf_LDADD = libpktio.a $(LIBCUNIT_COMMON) $(LIBODP)
