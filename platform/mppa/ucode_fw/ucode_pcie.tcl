######################################
# Copyright (C) 2008-2014 Kalray SA. #
#                                    #
# All rights reserved.               #
######################################

dma_description "PCIe microcode"
dma_description "arg0: number of 64 bits elements for packet 1"
dma_description "argx: number of 64 bits elements for packet x"
dma_description "arg7: number of 64 bits elements for packet 8"

dma_description "p0: address of packet 1"
dma_description "px: address of packet x"
dma_description "p7: address of packet 8"

for {set i 0} {$i < 8} {incr i} {
	set POINTER rd$i
	set SIZE64 r$i
	source ucode_pcie_main.tcl
}

dma_stop
dma_send_eot 0
dma_send_event
dma_write_bundle
