BUILDDIR   := $(shell pwd)
MAKEFILE   := $(shell echo $(MAKEFILE_LIST) | awk '{ print $$NF}')
TOP_SRCDIR := $(shell readlink -f $$(dirname $(MAKEFILE))/..)
SRCFILES   += $(shell find $(SRCDIRS) -name "[^.]*.c")
HDRFILES   += $(shell find $(SRCDIRS) -name "[^.]*.h")
OBJFILES   += $(patsubst $(TOP_SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCFILES))

_CFLAGS    += $(CFLAGS) -g -Wall -Wextra -Werror -std=gnu99 $(patsubst %, -I%, $(SRCDIRS))
_LDFLAGS   += $(LDFLAGS)

ifdef DEBUG
_CFLAGS    += -O0
else
_CFLAGS    += -O2
endif

BIN      := $(BUILDDIR)/$(FIRMDIR).kelf
INSTBIN  := $(INSTDIR)/$(FIRMDIR).kelf
all: $(BIN)

$(BIN): $(OBJFILES)
	$(CC) -o $@ $^ $(_CFLAGS) $(_LDFLAGS)
$(OBJFILES): $(BUILDDIR)/%.o: $(TOP_SRCDIR)/%.c $(HDRFILES)
	@mkdir -p $$(dirname $@) || true
	$(CC) -o $@ -c $< $(_CFLAGS)

install: $(INSTBIN)
$(INSTBIN): $(BIN)
	if [ "$(INSTDIR)" == "" ]; then echo "ERROR: INSTDIR not set" && exit 1; fi
	install -D $< $@

valid:
long:
