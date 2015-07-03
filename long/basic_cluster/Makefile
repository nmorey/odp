DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TEST_NAME := $(lastword $(subst /, ,$(DIR)))

$(TEST_NAME)-srcs := $(DIR)/cluster.c
$(TEST_NAME)-cflags := -DMODULE_HAS_OWN_MAIN=1

cluster-bin += $(TEST_NAME)
