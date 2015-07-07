all: build

ifdef DEBUG
DEBUG_CONF_FLAGS := --enable-debug
endif

TOP_DIR := $(shell readlink -f $$(pwd))
ARCH_DIR:= $(TOP_DIR)/build/
INST_DIR:= $(TOP_DIR)/install
K1ST_DIR:= $(INST_DIR)/local/k1tools/
MAKE_AMS:= $(shell find . -name Makefile.am)

FIRMWARES := boot ioeth
RULE_LIST := clean configure build install valid long
ARCH_COMPONENTS := odp cunit
COMPONENTS := extra doc $(ARCH_COMPONENTS) $(FIRMWARES)
CHECK_LIST :=

include platforms.inc

$(TOP_DIR)/configure: $(TOP_DIR)/bootstrap $(TOP_DIR)/configure.ac $(MAKE_AMS)
	cd $(TOP_DIR) && ./bootstrap
	@touch $@

$(TOP_DIR)/cunit/configure: $(TOP_DIR)/bootstrap
	cd $(TOP_DIR)/cunit/ && ./bootstrap
	@touch $@

define CONFIG_RULE
#Arg1 = machine name
$(1)-cunit-configure: $(TOP_DIR)/cunit/build/$(1)/Makefile

$(TOP_DIR)/cunit/build/$(1)/Makefile: $(TOP_DIR)/cunit/configure
	mkdir -p $$$$(dirname $$@) && cd $$$$(dirname $$@) && \
	$($(1)_CONF_ENV) CPPFLAGS="$($(1)_CFLAGS)" LDFLAGS="$($(1)_LDFLAGS) $($(1)_CFLAGS)" \
	 $$< --srcdir=$(TOP_DIR)/cunit --prefix=$(TOP_DIR)/cunit/install/$(1) \
	--enable-debug --enable-automated --enable-basic --host=$(1)

$(1)-cunit-build: $(TOP_DIR)/cunit/build/$(1)/CUnit/Sources/.libs/libcunit.a
$(TOP_DIR)/cunit/build/$(1)/CUnit/Sources/.libs/libcunit.a: $(TOP_DIR)/cunit/build/$(1)/Makefile
	$(MAKE) -C$(TOP_DIR)/cunit/build/$(1) V=1

$(1)-cunit-install: $(TOP_DIR)/cunit/install/$(1)/lib/libcunit.a
$(TOP_DIR)/cunit/install/$(1)/lib/libcunit.a: $(TOP_DIR)/cunit/build/$(1)/CUnit/Sources/.libs/libcunit.a
	$(MAKE) -C$(TOP_DIR)/cunit/build/$(1) V=1 install

$(1)-cunit-valid:
$(1)-cunit-long:
$(1)-cunit-clean:
	rm -Rf $(TOP_DIR)/cunit/build/$(1) $(TOP_DIR)/cunit/install/$(1)

$(1)-odp-configure: $(ARCH_DIR)/$(1)/Makefile
$(ARCH_DIR)/$(1)/Makefile: $(TOP_DIR)/configure $(TOP_DIR)/cunit/install/$(1)/lib/libcunit.a
	mkdir -p $$$$(dirname $$@) && cd $$$$(dirname $$@) && \
	$($(1)_CONF_ENV) CPPFLAGS="$($(1)_CFLAGS)" LDFLAGS="$($(1)_LDFLAGS) $($(1)_CFLAGS)" \
	 $$< --host=$(1) --with-platform=$($(1)_PLATFORM) \
	--with-cunit-path=$(TOP_DIR)/cunit/install/$(1)/ --enable-test-vald \
	--prefix=$(K1ST_DIR) \
	--datarootdir=$(K1ST_DIR)share/odp/$(1)/ \
	--libdir=$(K1ST_DIR)lib/$(1) \
	--includedir=$(K1ST_DIR)$($(1)_INC_DIR)/include \
	--enable-test-perf $(DEBUG_FLAGS) $($(1)_CONF_OPTS) $(DEBUG_CONF_FLAGS)

$(1)-odp-build: $(ARCH_DIR)/$(1)/Makefile
	$(MAKE) -C$(ARCH_DIR)/$(1)/ V=1 all

$(1)-odp-install: $(1)-odp-build
	if [ "$($(1)_INSTALL)" == "true" ]; then \
		$(MAKE) -C$(ARCH_DIR)/$(1) V=1 install-strip; \
	else true; fi
	if [ "$($(1)_INSTALL_DOC)" == "true" ]; then \
		$(MAKE) -C$(ARCH_DIR)/$(1) V=1 doxygen-pdf && \
		mkdir -p $(K1ST_DIR)/doc/ODP/ && \
		install $(ARCH_DIR)/$(1)/doc/output/opendataplane.pdf $(K1ST_DIR)/doc/ODP/opendataplane-$($(1)_PLATFORM).pdf && \
		rm -Rf   $(K1ST_DIR)/doc/ODP/opendataplane-$($(1)_PLATFORM) && \
		mkdir -p $(K1ST_DIR)/doc/ODP/opendataplane-$($(1)_PLATFORM) && \
		cp -Rf $(ARCH_DIR)/$(1)/doc/output/html/* $(K1ST_DIR)/doc/ODP/opendataplane-$($(1)_PLATFORM); \
	else true; fi

$(1)-odp-valid: $(1)-odp-build $(INST_DIR)/lib64/libodp_syscall.so
	$(MAKE) -C$(ARCH_DIR)/$(1)/test/validation $($(1)_MAKE_VALID) check
	$(MAKE) -C$(ARCH_DIR)/$(1)/platform/$($(1)_PLATFORM)/test/ check
	$(MAKE) -C$(ARCH_DIR)/$(1)/test/performance $($(1)_MAKE_VALID) check

$(1)-odp-clean:
	rm -Rf $(ARCH_DIR)/$(1)

$(1)-odp-long:
	$(error) 
	$(MAKE) -C$(TOP_DIR)/long O=$(ARCH_DIR)/long_$(1)/
endef

$(foreach CONFIG, $(_CONFIGS) $(CONFIGS), \
	$(eval $(call CONFIG_RULE,$(CONFIG))))


define FIRMWARE_RULE
#$(1) CONFIG
#$(2) Firmware type
$(eval $(1)-$(2)-ENV := CC="$($(1)_CC)" CFLAGS="$($(1)_CFLAGS)" \
	 LDFLAGS="$($(1)_LDFLAGS)" INSTDIR="$($(1)_INSTDIR)")
$(1)-$(2)-configure: firmware/$(2)/Makefile
$(1)-$(2)-build: firmware/$(2)/Makefile
	mkdir -p $(ARCH_DIR)/$(2)/$(1)
	cd $(ARCH_DIR)/$(2)/$(1) && make -f $(TOP_DIR)/firmware/$(2)/Makefile all $($(1)-$(2)-ENV)

$(1)-$(2)-install: $(1)-$(2)-build
	cd $(ARCH_DIR)/$(2)/$(1) && make -f $(TOP_DIR)/firmware/$(2)/Makefile install $($(1)-$(2)-ENV)
$(1)-$(2)-valid: $(1)-$(2)-build
	cd $(ARCH_DIR)/$(2)/$(1) && make -f $(TOP_DIR)/firmware/$(2)/Makefile valid $($(1)-$(2)-ENV)
$(1)-$(2)-long: $(1)-$(2)-build
	cd $(ARCH_DIR)/$(2)/$(1) && make -f $(TOP_DIR)/firmware/$(2)/Makefile long $($(1)-$(2)-ENV)
$(1)-$(2)-clean:
	rm -Rf $(ARCH_DIR)/$(2)/$(1)
endef

$(foreach FIRMWARE, $(FIRMWARES), \
	$(foreach CONFIG, $($(FIRMWARE)_CONFIGS), \
		$(eval $(call FIRMWARE_RULE,$(CONFIG),$(FIRMWARE)))))

list-configs:
	@echo $(CONFIGS)

doc-clean:
	$(MAKE) -C$(TOP_DIR)/doc-kalray clean
doc-configure:
doc-build:
doc-long:
doc-valid:
doc-install:
	$(MAKE) -C$(TOP_DIR)/doc-kalray install DOCDIR=$(K1ST_DIR)/doc/ODP/

extra-clean:
	rm -Rf $(TOP_DIR)/build $(INST_DIR) $(TOP_DIR)/configure $(TOP_DIR)/cunit/build/  $(TOP_DIR)/cunit/install $(TOP_DIR)/cunit/configure syscall/build_x86_64/
extra-configure:
extra-build: $(INST_DIR)/lib64/libodp_syscall.so
extra-valid:
extra-install: $(INST_DIR)/lib64/libodp_syscall.so example-install
extra-long:

example-install: x86_64-unknown-linux-gnu-odp-build
	mkdir -p $(K1ST_DIR)/doc/ODP/example/packet
	install example/example_debug.h platform/k1-cluster/test/pktio/pktio_env \
		example/packet/{odp_pktio.c,Makefile.k1a-kalray-nodeosmagic} \
		$(ARCH_DIR)/x86_64-unknown-linux-gnu/example/generator/odp_generator \
			$(K1ST_DIR)/doc/ODP/example

$(INST_DIR)/lib64/libodp_syscall.so: $(TOP_DIR)/syscall/run.sh
	+$< $(INST_DIR)/local/k1tools/

# Generate per config rules (clean/build/etc) for each firmware type
$(foreach RULE, $(RULE_LIST), \
	$(foreach FIRMWARE, $(FIRMWARES), \
		$(eval $(FIRMWARE)-$(RULE): $(foreach CONFIG, $($(FIRMWARE)_CONFIGS), $(CONFIG)-$(FIRMWARE)-$(RULE))) \
		$(eval CHECK_LIST += $(foreach CONFIG, $($(FIRMWARE)_CONFIGS), $(CONFIG)-$(FIRMWARE)-$(RULE))) \
	) \
	$(eval firmware-$(RULE): $(foreach FIRMWARE, $(FIRMWARES), $(FIRMWARE)-$(RULE))) \
	$(eval CHECK_LIST += $(foreach FIRMWARE, $(FIRMWARES), $(FIRMWARE)-$(RULE))) \
)
# Generate per config rules (clean/build/etc) for each arch specific component
$(foreach RULE, $(RULE_LIST), \
	$(foreach ARCH_COMPONENT, $(ARCH_COMPONENTS), \
		$(eval $(ARCH_COMPONENT)-$(RULE): $(foreach CONFIG, $(CONFIGS), $(CONFIG)-$(ARCH_COMPONENT)-$(RULE))) \
		$(eval CHECK_LIST += $(foreach CONFIG, $(CONFIGS), $(CONFIG)-$(ARCH_COMPONENT)-$(RULE))) \
))

# Generate global rules (clean/build/etc) calling all subcomponents
$(foreach RULE, $(RULE_LIST), \
		$(eval $(RULE): $(foreach COMPONENT, $(COMPONENTS), $(COMPONENT)-$(RULE))) \
		$(eval CHECK_LIST += $(foreach COMPONENT, $(COMPONENTS), $(COMPONENT)-$(RULE))) \
)


check-rules:
	@RULES=$$(valid/gen-rules.sh); MISSING=0 && \
	for rule in $(CHECK_LIST); do \
		echo $${RULES} | egrep -q "( |^)$${rule}( |$$)" || \
		{ \
			MISSING=1;\
			echo "Rule '$${rule}' missing"; \
		} \
	done; \
	[ $$MISSING -eq 0 ]
	@echo "check-rules OK"
