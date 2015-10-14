SRCS := ucode_eth.tcl ucode_pcie.tcl odp_ucode_linear.tcl
UCS  := $(patsubst %.tcl, %.c, $(SRCS))
all: $(UCS)

$(UCS): %.c : %.tcl
	k1-dma-ld $<

clean:
	rm -f $(UCS)
