SRCDIRS  += $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
_CFLAGS  += -I$(TOP_SRCDIR)/include
_LDFLAGS += -lmppapower -lmppanoc -lmpparouting -li2c -lphy

ifdef VERBOSE
_CFLAGS += -DVERBOSE
endif
