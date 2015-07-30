DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TEST_NAME := $(lastword $(subst /, ,$(DIR)))

$(TEST_NAME)-srcs := $(DIR)/cluster.c
cluster-bin += $(TEST_NAME)

$(TEST_NAME)_multibin-objs := $(BOOT_ELF) $(TEST_NAME)
mppa-bin += $(TEST_NAME)_multibin

$(TEST_NAME)-sim-cmd := boot -c $(TEST_NAME) -c $(TEST_NAME)
$(TEST_NAME)-jtag-cmd := boot -c $(TEST_NAME) -c $(TEST_NAME)
$(TEST_NAME)-multibinary := $(TEST_NAME)_multibin.mpk
mppa-tests += $(TEST_NAME)
