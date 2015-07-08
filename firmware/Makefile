BUILDDIR := $(shell pwd)

SRCFILES := $(shell find $(SRCDIR) -name "[^.]*.c")
HDRFILES := $(shell find $(SRCDIR) -name "[^.]*.h")
OBJFILES := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRCFILES))

_CFLAGS  := $(CFLAGS) -O2 -g -Wall -Wextra -Werror -std=gnu99
_LDFLAGS := $(LDFLAGS)

BIN      := $(BUILDDIR)/$(FIRMDIR).kelf
INSTBIN  := $(INSTDIR)/$(FIRMDIR).kelf
all: $(BIN)

$(BIN): $(OBJFILES)
	$(CC) -o $@ $^ $(_CFLAGS) $(_LDFLAGS)
$(OBJFILES): $(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HDRFILES)
	$(CC) -o $@ -c $< $(_CFLAGS)

install: $(INSTBIN)
$(INSTBIN): $(BIN)
	if [ "$(INSTDIR)" == "" ]; then echo "ERROR: INSTDIR not set" && exit 1; fi
	install -D $< $@

valid:
long:
