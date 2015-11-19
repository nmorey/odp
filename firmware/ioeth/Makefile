SRCDIR   := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
SRCDIRS  := $(SRCDIR)

_LDFLAGS = -lmppapower -lmppanoc -lmpparouting

include $(SRCDIR)/../common/rpc/include.mk
include $(SRCDIR)/../common/eth/include.mk
include $(SRCDIR)/../common/ioeth530-stack/include.mk
include $(SRCDIR)/../Makefile
