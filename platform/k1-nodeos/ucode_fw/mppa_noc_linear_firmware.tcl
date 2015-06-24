######################################
# Copyright (C) 2008-2014 Kalray SA. #
#                                    #
# All rights reserved.               #
######################################

dma_description "linear transfer"
dma_description "arg0: number of 64 bits elements"
dma_description "arg1: number of 8 bits elements (payload size)"
dma_description "arg2: destination offset"
dma_description "arg3: local offset"

## Load number of 64 bits elements to send.
dma_load dcnt0 r0
dma_write_bundle

dma_load rd1 r2 -header
dma_write_bundle

dma_read_header 0 rd1
dma_write_bundle

## Test if we must send 64 bits elements or not.
dma_load rd0 r3
dma_incr dcnt0
dma_bez dcnt0 send_bytes_label
dma_write_bundle

## Send 64 bits elements
dma_label send_64_bytes_loop_label
dma_incr dcnt0
dma_read_w64 0 rd0
dma_incr rd0
dma_bnz dcnt0 send_64_bytes_loop_label
dma_write_bundle

## Load number of 8 bits elements to send.
dma_label send_bytes_label
# Necessary to avoid, on Bostan, bug 10458 which needs a flush after
# sending aligned buffer.
dma_flush 0
dma_load dcnt0 r1
dma_write_bundle

## Test if we must send 8 bits elements or not.
dma_incr dcnt0
dma_bez dcnt0 end_label
dma_write_bundle

## Send 8 bits elements
dma_label send_bytes_loop_label
dma_incr dcnt0
dma_read_w8 0 rd0
dma_incr rd0
dma_bnz dcnt0 send_bytes_loop_label
dma_write_bundle

dma_label end_label
