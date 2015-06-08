all: build

TOP_DIR := $(shell readlink -f $$(pwd))
ARCH_DIR:= $(TOP_DIR)/build/
INST_DIR:= $(TOP_DIR)/install
K1ST_DIR:= $(INST_DIR)/local/k1tools/
CONFIGS :=

k1a-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++
k1a-kalray-nodeos_CONF_OPTS   :=
k1a-kalray-nodeos_PLATFORM    := k1-nodeos
k1a-kalray-nodeos_MAKE_PLAT   :=
k1a-kalray-nodeos_BUILD_TESTS := true
k1a-kalray-nodeos_INSTALL     := true
CONFIGS += k1a-kalray-nodeos

k1a-kalray-nodeosmagic_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++
k1a-kalray-nodeosmagic_CONF_OPTS   :=
k1a-kalray-nodeosmagic_PLATFORM    := k1-nodeos
k1a-kalray-nodeosmagic_MAKE_PLAT   :=
k1a-kalray-nodeosmagic_BUILD_TESTS := true
k1a-kalray-nodeosmagic_INSTALL     := true
CONFIGS += k1a-kalray-nodeosmagic

k1b-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++
k1b-kalray-nodeos_CONF_OPTS   :=
k1b-kalray-nodeos_PLATFORM    := k1-nodeos
k1b-kalray-nodeos_MAKE_PLAT   :=
k1b-kalray-nodeos_BUILD_TESTS := true
k1b-kalray-nodeos_INSTALL     := true
CONFIGS += k1b-kalray-nodeos

# k1b-kalray-nodeosmagic_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++
# k1b-kalray-nodeosmagic_CONF_OPTS   :=
# k1b-kalray-nodeosmagic_PLATFORM    := k1-nodeos
# k1b-kalray-nodeosmagic_MAKE_PLAT   :=
# k1b-kalray-nodeosmagic_BUILD_TESTS := true
# k1b-kalray-nodeosmagic_INSTALL     := true
# CONFIGS += k1b-kalray-nodeosmagic

x86_64-unknown-linux-gnu_CONF_ENV    :=
x86_64-unknown-linux-gnu_CONF_OPTS   :=
x86_64-unknown-linux-gnu_PLATFORM    := linux-generic
x86_64-unknown-linux-gnu_MAKE_PLAT   :=
x86_64-unknown-linux-gnu_BUILD_TESTS := false
x86_64-unknown-linux-gnu_INSTALL     := false
CONFIGS += x86_64-unknown-linux-gnu

$(TOP_DIR)/configure: $(TOP_DIR)/bootstrap $(TOP_DIR)/configure.ac
	cd $(TOP_DIR) && ./bootstrap

$(TOP_DIR)/cunit/configure: $(TOP_DIR)/bootstrap
	cd $(TOP_DIR)/cunit/ && ./bootstrap

define CONFIG_RULE
#Arg1 = machine name
$(1)-cunit-configure: $(TOP_DIR)/cunit/build/$(1)/Makefile

$(TOP_DIR)/cunit/build/$(1)/Makefile: $(TOP_DIR)/cunit/configure
	mkdir -p $$$$(dirname $$@) && cd $$$$(dirname $$@) && \
	$($(1)_CONF_ENV) $$< --srcdir=$(TOP_DIR)/cunit --prefix=$(TOP_DIR)/cunit/install/$(1) \
	--enable-debug --enable-automated --enable-basic --enable-console \
	--enable-examples --enable-test --host=$(1)

$(1)-cunit-build: $(TOP_DIR)/cunit/build/$(1)/CUnit/Sources/.libs/libcunit.a
$(TOP_DIR)/cunit/build/$(1)/CUnit/Sources/.libs/libcunit.a: $(TOP_DIR)/cunit/build/$(1)/Makefile
	$(MAKE) -C$(TOP_DIR)/cunit/build/$(1) V=1

$(1)-cunit-install: $(TOP_DIR)/cunit/install/$(1)/lib/libcunit.a
$(TOP_DIR)/cunit/install/$(1)/lib/libcunit.a: $(TOP_DIR)/cunit/build/$(1)/CUnit/Sources/.libs/libcunit.a
	$(MAKE) -C$(TOP_DIR)/cunit/build/$(1) V=1 install

$(1)-cunit-valid:
$(1)-cunit-clean:
	rm -Rf $(TOP_DIR)/cunit/build/$(1) $(TOP_DIR)/cunit/install/$(1)

$(1)-odp-configure: $(ARCH_DIR)/$(1)/Makefile
$(ARCH_DIR)/$(1)/Makefile: $(TOP_DIR)/configure $(TOP_DIR)/cunit/install/$(1)/lib/libcunit.a
	mkdir -p $$$$(dirname $$@) && cd $$$$(dirname $$@) && \
	$($(1)_CONF_ENV) $$< --host=$(1) --with-platform=$($(1)_PLATFORM) \
	--with-cunit-path=$(TOP_DIR)/cunit/install/$(1)/ --enable-test-vald \
	--prefix=$(K1ST_DIR) \
	--libdir=$(K1ST_DIR)lib/$(1) \
	--include=$(K1ST_DIR)$($(1)_PLATFORM)/include \
	--enable-test-perf $(DEBUG_FLAGS) $($(1)_CONF_OPTS)

$(1)-odp-build: $(ARCH_DIR)/$(1)/Makefile
	$(MAKE) -C$(ARCH_DIR)/$(1)/platform V=1 all
	if [ "$($(1)_BUILD_TESTS)" == "true" ]; then \
		$(MAKE) -C$(ARCH_DIR)/$(1)/test V=1 all; \
	else true; fi
		$(MAKE) -C$(ARCH_DIR)/$(1)/example/generator V=1 all; \

$(1)-odp-install: $(1)-odp-build
	if [ "$($(1)_INSTALL)" == "true" ]; then \
		$(MAKE) -C$(ARCH_DIR)/$(1) V=1 install && \
		$(MAKE) -C$(ARCH_DIR)/$(1) V=1 doxygen-pdf &&  \
		install $(ARCH_DIR)/$(1)/doc/output/opendataplane.pdf $(K1ST_DIR)/doc/ODP/opendataplane-$($(1)_PLATFORM).pdf; \
	else true; fi

$(1)-odp-valid: $(1)-odp-build $(TOP_DIR)/install/lib64/libodp_syscall.so
	if [ "$($(1)_BUILD_TESTS)" == "true" ]; then \
		$(MAKE) -C$(ARCH_DIR)/$(1)/test/validation -j1 check && \
		$(MAKE) -C$(ARCH_DIR)/$(1)/test/performance -j1 check; \
	else true; fi

$(1)-odp-clean:
	rm -Rf $(ARCH_DIR)/$(1)
endef

$(foreach CONFIG, $(CONFIGS), \
	$(eval $(call CONFIG_RULE,$(CONFIG))))

list-configs:
	@echo $(CONFIGS)

doc-clean:
	$(MAKE) -C$(TOP_DIR)/doc-kalray clean
doc-configure:
doc-build:
doc-valid:
doc-install:
	$(MAKE) -C$(TOP_DIR)/doc-kalray install DOCDIR=$(K1ST_DIR)/doc/ODP/

extra-clean:
	rm -Rf $(TOP_DIR)/build $(TOP_DIR)/install $(TOP_DIR)/configure $(TOP_DIR)/cunit/build/  $(TOP_DIR)/cunit/install $(TOP_DIR)/cunit/configure
extra-configure:
extra-build: $(TOP_DIR)/install/lib64/libodp_syscall.so
extra-valid:
extra-install: $(TOP_DIR)/install/lib64/libodp_syscall.so

$(TOP_DIR)/install/lib64/libodp_syscall.so: $(TOP_DIR)/syscall/run.sh
	$< $(TOP_DIR)/install/

RULE_LIST := clean configure build install valid
ARCH_COMPONENTS := odp cunit
$(foreach RULE, $(RULE_LIST), \
	$(foreach ARCH_COMPONENT, $(ARCH_COMPONENTS), \
		$(eval $(ARCH_COMPONENT)-$(RULE): $(foreach CONFIG, $(CONFIGS), $(CONFIG)-$(ARCH_COMPONENT)-$(RULE)))))

COMPONENTS := extra doc $(ARCH_COMPONENTS)
$(foreach RULE, $(RULE_LIST), \
		$(eval $(RULE): $(foreach COMPONENT, $(COMPONENTS), $(COMPONENT)-$(RULE))))


