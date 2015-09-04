DIR := $(dir $(lastword $(MAKEFILE_LIST)))
TEST_NAME := $(lastword $(subst /, ,$(DIR)))

$(TEST_NAME)-srcs := $(DIR)/ring_com.c
cluster-bin += $(TEST_NAME)

$(TEST_NAME)_multibin-objs := $(BOOT_ELF) $(TEST_NAME)
mppa-bin += $(TEST_NAME)_multibin

$(TEST_NAME)-sim-cmd := $(TEST_NAME)_multibin.mpk $(foreach i,$(shell seq 16),-c $(TEST_NAME))
#~ mppa-tests += $(TEST_NAME)
