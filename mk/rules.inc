#
# Define all rules for a given config for odp and CUNIT
#
define CONFIG_RULE
#Arg1 = machine name
$(1)-cunit-configure: $(TOP_DIR)/cunit/build/$(1)/Makefile

$(TOP_DIR)/cunit/build/$(1)/Makefile: $(TOP_DIR)/cunit/configure
	mkdir -p $$$$(dirname $$@) && cd $$$$(dirname $$@) && \
	$($(1)_CONF_ENV) CPPFLAGS="$($(1)_CFLAGS)" LDFLAGS="$($(1)_LDFLAGS) $($(1)_CFLAGS)" \
	 $$< --srcdir=$(TOP_DIR)/cunit --prefix=$(CUNIT_INST_DIR)/$(1) \
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
	--with-cunit-path=$(CUNIT_INST_DIR)/$(1)/ --enable-test-vald \
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

#
# Define all rules for a given config/firmware
#
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


#
# Define rule $(1) for each subtype (arch, firmware, etc.)
#
define SUB_RULES
# $(1) = RULE
# Generate per config rules (clean/build/etc) for each firmware type
$(foreach FIRMWARE, $(FIRMWARES),
$(FIRMWARE)-$(1): $(foreach CONFIG, $($(FIRMWARE)_CONFIGS), $(CONFIG)-$(FIRMWARE)-$(1))
CHECK_LIST += $(foreach CONFIG, $($(FIRMWARE)_CONFIGS), $(CONFIG)-$(FIRMWARE)-$(1))
)

firmware-$(1): $(foreach FIRMWARE, $(FIRMWARES), $(FIRMWARE)-$(1))
CHECK_LIST += $(foreach FIRMWARE, $(FIRMWARES), $(FIRMWARE)-$(1))

# Generate per config rules (clean/build/etc) for each arch specific component
$(foreach ARCH_COMPONENT, $(ARCH_COMPONENTS), \
$(ARCH_COMPONENT)-$(1): $(foreach CONFIG, $(CONFIGS), $(CONFIG)-$(ARCH_COMPONENT)-$(1))
CHECK_LIST += $(foreach CONFIG, $(CONFIGS), $(CONFIG)-$(ARCH_COMPONENT)-$(1))
)
endef

#
# Global rules for parallel actions
#
define DO_RULES_PAR
# $(1) = RULE
$(1): $(foreach COMPONENT, $(COMPONENTS), $(COMPONENT)-$(1))
CHECK_LIST += $(foreach COMPONENT, $(COMPONENTS),$(COMPONENT)-$(1))
endef

#
# Global rules for sequential actions
#
define DO_RULES_SEQ
# $(1) = RULE
.PHONY: $(1)
$(1):
	for rule in $(foreach COMPONENT, $(COMPONENTS), $(COMPONENT)-$(1)); do $(MAKE) $$$${rule}; done

CHECK_LIST += $(foreach COMPONENT, $(COMPONENTS),$(COMPONENT)-$(1))
endef
