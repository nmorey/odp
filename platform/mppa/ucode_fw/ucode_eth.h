/** \brief ucode_eth
 * Ethernet microcode
 * arg0: number of 64 bits elements for packet 1
 * arg1: number of 8 bits elements (payload size) for packet 1
 * arg2: number of 64 bits elements for packet 2
 * arg3: number of 8 bits elements (payload size) for packet 2
 * arg4: number of 64 bits elements for packet 3
 * arg5: number of 8 bits elements (payload size) for packet 3
 * arg6: number of 64 bits elements for packet 4
 * arg7: number of 8 bits elements (payload size) for packet 4
 * p0: address of packet 1
 * p1: address of packet 2
 * p2: address of packet 3
 * p3: address of packet 4
 */
extern unsigned long long ucode_eth[] __attribute__((aligned(128) ));

