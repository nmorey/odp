all: build

ifdef DEBUG
DEBUG_CONF_FLAGS := --enable-debug
endif

TOP_DIR := $(shell readlink -f $$(pwd))
ARCH_DIR:= $(TOP_DIR)/build/
INST_DIR:= $(TOP_DIR)/install
K1ST_DIR:= $(INST_DIR)/local/k1tools/
CUNIT_INST_DIR:= $(TOP_DIR)/cunit/install/local/k1tools/kalray_internal/cunit/
MAKE_AMS:= $(shell find $(TOP_DIR) -name Makefile.am)
MAKE_DEPS:= $(MAKE_AMS) $(TOP_DIR)/Makefile $(wildcard $(TOP_DIR)/mk/*.inc)

FIRMWARES := boot ioeth iounified
RULE_LIST_SERIAL   :=  install valid long
RULE_LIST_PARALLEL := clean configure build
RULE_LIST := $(RULE_LIST_SERIAL) $(RULE_LIST_PARALLEL)
ARCH_COMPONENTS := odp cunit
COMPONENTS := extra doc $(ARCH_COMPONENTS) $(FIRMWARES)
CHECK_LIST :=

install_DEPS := build

include mk/platforms.inc
include mk/rules.inc

$(TOP_DIR)/configure: $(TOP_DIR)/bootstrap $(TOP_DIR)/configure.ac $(MAKE_AMS)
	cd $(TOP_DIR) && ./bootstrap
	@touch $@

$(TOP_DIR)/cunit/configure: $(TOP_DIR)/bootstrap
	cd $(TOP_DIR)/cunit/ && ./bootstrap
	@touch $@

#
# Define CUNIT/ODP rules for all targets
#
$(foreach CONFIG, $(_CONFIGS) $(CONFIGS), \
	$(eval $(call CONFIG_RULE,$(CONFIG))))

#
# Define firmware rules for all firmwares and all their targets
#
$(foreach FIRMWARE, $(FIRMWARES), \
	$(foreach CONFIG, $($(FIRMWARE)_CONFIGS), \
		$(eval $(call FIRMWARE_RULE,$(CONFIG),$(FIRMWARE)))))

#
# Documentation rules
#
doc-clean:
	$(MAKE) -C$(TOP_DIR)/doc-kalray clean
doc-configure:
doc-build:
doc-long:
doc-valid:
doc-install:
	$(MAKE) -C$(TOP_DIR)/doc-kalray install DOCDIR=$(K1ST_DIR)/doc/ODP/

#
# Extra rules:
#  * Cleanup all
#  * Magic syscall lib
#
extra-clean:
	rm -Rf $(TOP_DIR)/build $(INST_DIR) $(TOP_DIR)/configure $(TOP_DIR)/cunit/build/ \
	 $(TOP_DIR)/cunit/install $(TOP_DIR)/cunit/configure syscall/build_x86_64/
extra-configure:
extra-build: $(INST_DIR)/lib64/libodp_syscall.so
extra-valid:
extra-install: $(INST_DIR)/lib64/libodp_syscall.so example-install
extra-long:
example-install: x86_64-unknown-linux-gnu-odp-build
	mkdir -p $(K1ST_DIR)/doc/ODP/example/packet
	install example/example_debug.h platform/mppa/test/pktio/pktio_env \
		example/packet/{odp_pktio.c,Makefile.k1a-kalray-nodeos_simu} \
		$(ARCH_DIR)/x86_64-unknown-linux-gnu/example/generator/odp_generator \
			$(K1ST_DIR)/doc/ODP/example
$(INST_DIR)/lib64/libodp_syscall.so: $(TOP_DIR)/syscall/run.sh
	+$< $(INST_DIR)/local/k1tools/


#
# Generate rule wrappers that pull all CONFIGS for a given (firmware/Arch componen)|RULE
#
$(foreach RULE, $(RULE_LIST), $(eval $(call SUB_RULES,$(RULE))))


#
# Generate rule wrappers that pull all (firmware/Arch Component)/CONFIGS for a given RULE
#
$(foreach RULE, $(RULE_LIST_PARALLEL), $(eval $(call DO_RULES_PAR,$(RULE))))
$(foreach RULE, $(RULE_LIST_SERIAL), $(eval $(call DO_RULES_SEQ,$(RULE))))


check-rules:
	@echo $(CHECK_LIST)
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

junits:
	@[ "$(JUNIT_FILE)" != "" ] || { echo "JUNIT_FILE not set"; exit 1; }
	@(FLIST=$$(find . -name "*.trs") && \
	echo -e "<testsuite errors=\"0\" skipped=\"0\" failures=\"0\" time=\"0\" tests=\"0\" name=\"cunit\""\
	" hostname=\"$$(hostname)\">\n\t<properties>\n" \
	"\t\t<property value=\"$$(hostname)\" name=\"host.name\" />\n" \
	"\t\t<property value=\"$$(uname -m)\" name=\"host.kernel.arch\" />\n" \
	"\t\t<property value=\"$$(uname -r)\" name=\"host.kernel.release\" />\n" \
	"\t</properties>\n" && \
	for file in $$FLIST; do \
		TNAME=$$(echo $$file | sed -e 's/.trs//' -e 's@/@_@g' -e 's/^[._]*//' ) && \
		FNAME=$${file%.trs} && \
		echo -e "\t<testcase classname=\"cunit.tests\" name=\"cunit.tests.$${TNAME}\">" &&\
		status=$$(grep :test-result: $$file | awk '{ print $$NF}') && \
		if [ "$$status" == "FAIL" || "$$status" == "XFAIL" ]; then \
			echo -e "\t\t<error message=\"cunit.tests failure\" type=\"Error\">";\
		else \
			echo -e "\t\t<system-out>"; \
		fi; \
		if [ -f $$FNAME.log ]; then \
			echo -e "\t\t\t<![CDATA[" && \
			cat $$FNAME.log && \
			echo -e "\t\t\t]]>"; \
		fi; \
		if [ "$$status" == "FAIL" || "$$status" == "XFAIL" ]; then \
			echo -e "\t\t</error>";\
		else \
			echo -e "\t\t</system-out>"; \
		fi; \
		echo -e "\t</testcase>" ;\
	done && \
	echo "</testsuite>") > $(JUNIT_FILE)

list-configs:
	@echo $(CONFIGS)

