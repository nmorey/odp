/** \brief odp_ucode_linear
 * linear transfer
 * arg0: number of 64 bits elements
 * arg1: number of 8 bits elements (payload size)
 * arg2: destination offset
 * arg3: local offset
 * arg4: packet header size (number of 64 bits elements)
 * arg5: packet header addr
 */
extern unsigned long long odp_ucode_linear[] __attribute__((aligned(128) ));

