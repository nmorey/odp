######################################
# Copyright (C) 2008-2014 Kalray SA. #
#                                    #
# All rights reserved.               #
######################################

dma_description "Ethernet microcode"
dma_description "arg0: number of 64 bits elements for packet 1"
dma_description "arg1: number of 8 bits elements (payload size) for packet 1"
dma_description "arg2: number of 64 bits elements for packet 2"
dma_description "arg3: number of 8 bits elements (payload size) for packet 2"
dma_description "arg4: number of 64 bits elements for packet 3"
dma_description "arg5: number of 8 bits elements (payload size) for packet 3"
dma_description "arg6: number of 64 bits elements for packet 4"
dma_description "arg7: number of 8 bits elements (payload size) for packet 4"

dma_description "p0: address of packet 1"
dma_description "p1: address of packet 2"
dma_description "p2: address of packet 3"
dma_description "p3: address of packet 4"


for {set i 0} {$i < 4} {incr i} {
	set POINTER rd$i
	set SIZE64 r[expr $i * 2]
	set SIZE8 r[expr $i * 2 + 1]
	source ucode_eth_main.tcl
}

dma_stop
dma_send_event
dma_write_bundle
