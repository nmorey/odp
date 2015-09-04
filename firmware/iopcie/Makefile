SRCDIR   := $(shell dirname $$(echo $(MAKEFILE_LIST) | awk '{ print $$NF}'))
SRCDIRS  := $(SRCDIR)

_LDFLAGS = -lmppapower -lmppanoc -lmpparouting -T $(SRCDIR)/linker.ld

include $(SRCDIR)/../common/rpc/include.mk
include $(SRCDIR)/../Makefile.firmware
