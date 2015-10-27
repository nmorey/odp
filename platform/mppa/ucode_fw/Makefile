SRCS := ucode_eth.tcl ucode_pcie.tcl odp_ucode_linear.tcl ucode_eth_v2.tcl
UCS  := $(patsubst %.tcl, %.c, $(SRCS))
all: $(UCS)

$(UCS): %.c : %.tcl
	k1-dma-ld -x $<

clean:
	rm -f $(UCS)
