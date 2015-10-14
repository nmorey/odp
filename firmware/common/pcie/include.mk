SRCDIRS  += $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
_LDFLAGS += -Wl,--undefined=__pcie_rpc_constructor
