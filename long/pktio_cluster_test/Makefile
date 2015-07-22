DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TEST_NAME := $(lastword $(subst /, ,$(DIR)))

$(TEST_NAME)-srcs := $(DIR)/cluster.c $(CUNIT_COMMON_SRC)
$(TEST_NAME)-cflags := -DMODULE_HAS_OWN_MAIN=1

$(TEST_NAME)-sim-cmd := $(TEST_NAME)
$(TEST_NAME)-jtag-cmd := $(TEST_NAME)

cluster-tests += $(TEST_NAME)
cluster-bin += $(TEST_NAME)
