/** \brief ucode_pcie
 * PCIe microcode
 * arg0: number of 64 bits elements for packet 1
 * argx: number of 64 bits elements for packet x
 * arg7: number of 64 bits elements for packet 8
 * p0: address of packet 1
 * px: address of packet x
 * p7: address of packet 8
 */
extern unsigned long long ucode_pcie[] __attribute__((aligned(128) ));

