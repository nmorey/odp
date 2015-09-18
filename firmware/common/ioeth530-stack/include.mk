SRCDIRS  += $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
_LDFLAGS += -Wl,--defsym,_K1_PE_STACK_ADDRESS=__rm4_stack_start -Wl,--allow-multiple-definition
