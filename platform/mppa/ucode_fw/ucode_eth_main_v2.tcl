
## POINTER is the local read pointer
## SIZE64 is the parameters containing the 64 bits size of the payload
## SIZE8 is the parameters containing the 8 bits size of the payload

## Load number of 64 bits elements to send.
dma_load dcnt0 ${SIZE64}
dma_write_bundle

## Test if we must send 64 bits elements or not.
dma_decr dcnt0
dma_bez dcnt0 send_bytes_label${POINTER}
dma_write_bundle

## Send 64 bits elements
dma_label send_64_bytes_loop_label${POINTER}
dma_decr dcnt0
dma_read_w64 0 ${POINTER}
dma_decr ${POINTER}
dma_bnz dcnt0 send_64_bytes_loop_label${POINTER}
dma_write_bundle

## Load number of 8 bits elements to send.
dma_label send_bytes_label${POINTER}
# Necessary to avoid, on Bostan, bug 10458 which needs a flush after
# sending aligned buffer.
dma_flush 0
dma_load dcnt0 ${SIZE8}
dma_write_bundle

## Test if we must send 8 bits elements or not.
dma_decr dcnt0
dma_bez dcnt0 end_label${POINTER}
dma_write_bundle

## Send 8 bits elements
dma_label send_bytes_loop_label${POINTER}
dma_decr dcnt0
dma_read_w8 0 ${POINTER}
dma_decr ${POINTER}
dma_bnz dcnt0 send_bytes_loop_label${POINTER}
dma_write_bundle

dma_label end_label${POINTER}



dma_send_eot 0
if { $i == 3 } {
	dma_goto high_speed_loop
}
dma_write_bundle

dma_label skip_label${POINTER}
