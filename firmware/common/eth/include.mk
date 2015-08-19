SRCDIRS  += $(shell dirname $$(echo $(MAKEFILE_LIST) | awk '{ print $$NF}'))
_CFLAGS  += -I$(TOP_SRCDIR)/include
_LDFLAGS += -lmppapower -lmppanoc -lmpparouting -li2c -lphy
