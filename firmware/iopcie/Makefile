SRCDIR   := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
SRCDIRS  := $(SRCDIR)

_LDFLAGS = -lmppapower -lmppanoc -lmpparouting -T $(SRCDIR)/linker.ld

include $(SRCDIR)/../common/rpc/include.mk
include $(SRCDIR)/../common/pcie/include.mk
include $(SRCDIR)/../common/boot/include.mk
include $(SRCDIR)/../Makefile.firmware
