
## POINTER is the local read pointer
## SIZE64 is the parameters containing the 64 bits size of the payload
## SIZE8 is the parameters containing the 8 bits size of the payload

## Load number of 64 bits elements to send.
dma_load dcnt0 ${SIZE64}
dma_write_bundle

## Load number of remaining bytes to send
dma_load dcnt1 ${SIZE8}
dma_write_bundle

## If there is nothing under 8 bytes, special send to send EOT packed with the elements
dma_bez dcnt1 norest_label${POINTER}
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

# Necessary to avoid, on Bostan, bug 10458 which needs a flush after
# sending aligned buffer.
dma_flush 0
dma_write_bundle

## Under 8 bytes handling
dma_label send_bytes_label${POINTER}
dma_decr dcnt1
dma_write_bundle

## Send 8 bits elements
dma_label send_bytes_loop_label${POINTER}
dma_decr dcnt1
dma_read_w8 0 ${POINTER}
dma_decr ${POINTER}
dma_bnz dcnt1 send_bytes_loop_label${POINTER}
dma_write_bundle

## Send EOT after packet + bytes
dma_goto end_label${POINTER}
dma_send_eot 0
dma_write_bundle

#
# Only 8-bytes stuff
# Go to the end if size is 0
#
dma_label norest_label${POINTER}
dma_bez dcnt0 end_label${POINTER}
dma_decr dcnt0
dma_write_bundle

## Decr number of 8B twice because we send the last one manually
dma_decr dcnt0
dma_write_bundle

## Send 64 bits elements
dma_label mult_send_64_bytes_loop_label${POINTER}
dma_decr dcnt0
dma_read_w64 0 ${POINTER}
dma_decr ${POINTER}
dma_bnz dcnt0 mult_send_64_bytes_loop_label${POINTER}
dma_write_bundle

## Send the last 8B with an EOT
dma_read_w64 0 ${POINTER}
dma_decr ${POINTER}
dma_send_eot 0
dma_write_bundle

dma_label end_label${POINTER}
