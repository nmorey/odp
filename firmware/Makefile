FIRMDIR    := $(shell basename $(SRCDIR))
BUILDDIR   := $(shell pwd)
MAKEFILE   := $(lastword $(MAKEFILE_LIST))
TOP_SRCDIR := $(realpath $(dir $(MAKEFILE))/..)
SRCFILES   += $(shell find $(SRCDIRS) -name "[^.]*.c")
HDRFILES   += $(shell find $(SRCDIRS) -name "[^.]*.h")
OBJFILES   += $(patsubst $(TOP_SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCFILES))

ODP_INCDIR  = $(TOP_SRCDIR)/platform/mppa/include
ODP_SRCDIR  = $(TOP_SRCDIR)/platform/mppa/
_CFLAGS    += -I$(ODP_INCDIR) -I$(TOP_SRCDIR)/include
_CFLAGS    += $(CFLAGS) -g3 -Wall -Wextra -Werror -std=gnu99 $(patsubst %, -I%, $(SRCDIRS))
_LDFLAGS   += $(LDFLAGS)

ifdef DEBUG
_CFLAGS    += -O0
else
_CFLAGS    += -O2
endif
ifdef VERBOSE
_CFLAGS += -DVERBOSE
endif

ifndef INCLUDING_FIRMWARE

ifdef BUILD_LIBRARY
LIB      := $(BUILDDIR)/lib$(FIRMDIR).a
INSTBIN  := $(INSTDIR)/lib$(FIRMDIR).a
else
BIN      := $(BUILDDIR)/$(FIRMDIR).kelf
INSTBIN  := $(INSTDIR)/$(FIRMDIR).kelf
endif
all: $(BIN) $(LIB)

$(BIN): $(OBJFILES)
	$(CC) -o $@ $^ $(_CFLAGS) $(_LDFLAGS)
$(LIB): $(OBJFILES)
	ar rc $@ $^

$(OBJFILES): $(BUILDDIR)/%.o: $(TOP_SRCDIR)/%.c $(HDRFILES)
	@mkdir -p $$(dirname $@) || true
	$(CC) -o $@ -c $< $(_CFLAGS)

install: $(INSTBIN) $(INSTLIB)
$(INSTBIN): $(BIN)
	if [ "$(INSTDIR)" == "" ]; then echo "ERROR: INSTDIR not set" && exit 1; fi
	install -D $< $@
$(INSTLIB): $(LIB)
	if [ "$(INSTDIR)" == "" ]; then echo "ERROR: INSTDIR not set" && exit 1; fi
	install -D $< $@

valid:
long:

clean:
	rm -f $(OBJFILES) $(BIN)
endif
