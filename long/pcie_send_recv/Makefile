DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TEST_NAME := $(lastword $(subst /, ,$(DIR)))

$(TEST_NAME)-srcs := $(DIR)/main.c
cluster-bin += $(TEST_NAME)

$(TEST_NAME)_multibin-objs := $(IOPCIE_ELF) $(TEST_NAME)
mppa-bin += $(TEST_NAME)_multibin

$(TEST_NAME)-sim-cmd := $(IOPCIE_NAME) -c $(TEST_NAME)
$(TEST_NAME)-jtag-cmd := $(IOPCIE_NAME) -c $(TEST_NAME)
$(TEST_NAME)-multibinary := $(TEST_NAME)_multibin.mpk
mppa-tests += $(TEST_NAME)
