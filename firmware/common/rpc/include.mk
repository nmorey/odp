SRCDIRS  += $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
_CFLAGS  += -I$(TOP_SRCDIR)/include
_LDFLAGS += -Wl,--undefined=__bas_rpc_constructor
HDRFILES += $(ODP_INCDIR)/odp_rpc_internal.h
SRCFILES += $(ODP_SRCDIR)/odp_rpc.c
