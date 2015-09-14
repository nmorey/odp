/** \brief ucode_pcie
 * PCIe microcode
 * arg0: number of 64 bits elements for packet 1
 * argx: number of 64 bits elements for packet x
 * arg7: number of 64 bits elements for packet 8
 * p0: address of packet 1
 * px: address of packet x
 * p7: address of packet 8
 */
unsigned long long ucode_pcie[] __attribute__((aligned(128) )) = {
0x0000001000600000ULL,  /* C_0: dcnt0=R[0];*/
0x0000000000400012ULL,  /* C_1: if(dcnt0==0) goto C_4; dcnt0--;*/
0x0000000803c0000bULL,  /* C_2: READ8(ptr_0,chan_0); ptr_0+=8; if(dcnt0!=0) goto C_2; dcnt0--;*/
0x0000000000004000ULL,  /* C_3: SEND_EOT(chan_0);*/
0x0000003000600000ULL,  /* C_4: dcnt0=R[1];*/
0x0000000000400022ULL,  /* C_5: if(dcnt0==0) goto C_8; dcnt0--;*/
0x0000000843c0001bULL,  /* C_6: READ8(ptr_1,chan_0); ptr_1+=8; if(dcnt0!=0) goto C_6; dcnt0--;*/
0x0000000000004000ULL,  /* C_7: SEND_EOT(chan_0);*/
0x0000005000600000ULL,  /* C_8: dcnt0=R[2];*/
0x0000000000400032ULL,  /* C_9: if(dcnt0==0) goto C_12; dcnt0--;*/
0x0000000883c0002bULL,  /* C_10: READ8(ptr_2,chan_0); ptr_2+=8; if(dcnt0!=0) goto C_10; dcnt0--;*/
0x0000000000004000ULL,  /* C_11: SEND_EOT(chan_0);*/
0x0000007000600000ULL,  /* C_12: dcnt0=R[3];*/
0x0000000000400042ULL,  /* C_13: if(dcnt0==0) goto C_16; dcnt0--;*/
0x00000008c3c0003bULL,  /* C_14: READ8(ptr_3,chan_0); ptr_3+=8; if(dcnt0!=0) goto C_14; dcnt0--;*/
0x0000000000004000ULL,  /* C_15: SEND_EOT(chan_0);*/
0x0000009000600000ULL,  /* C_16: dcnt0=R[4];*/
0x0000000000400052ULL,  /* C_17: if(dcnt0==0) goto C_20; dcnt0--;*/
0x0000000903c0004bULL,  /* C_18: READ8(ptr_4,chan_0); ptr_4+=8; if(dcnt0!=0) goto C_18; dcnt0--;*/
0x0000000000004000ULL,  /* C_19: SEND_EOT(chan_0);*/
0x000000b000600000ULL,  /* C_20: dcnt0=R[5];*/
0x0000000000400062ULL,  /* C_21: if(dcnt0==0) goto C_24; dcnt0--;*/
0x0000000943c0005bULL,  /* C_22: READ8(ptr_5,chan_0); ptr_5+=8; if(dcnt0!=0) goto C_22; dcnt0--;*/
0x0000000000004000ULL,  /* C_23: SEND_EOT(chan_0);*/
0x000000d000600000ULL,  /* C_24: dcnt0=R[6];*/
0x0000000000400072ULL,  /* C_25: if(dcnt0==0) goto C_28; dcnt0--;*/
0x0000000983c0006bULL,  /* C_26: READ8(ptr_6,chan_0); ptr_6+=8; if(dcnt0!=0) goto C_26; dcnt0--;*/
0x0000000000004000ULL,  /* C_27: SEND_EOT(chan_0);*/
0x000000f000600000ULL,  /* C_28: dcnt0=R[7];*/
0x0000000000400082ULL,  /* C_29: if(dcnt0==0) goto C_32; dcnt0--;*/
0x00000009c3c0007bULL,  /* C_30: READ8(ptr_7,chan_0); ptr_7+=8; if(dcnt0!=0) goto C_30; dcnt0--;*/
0x0000000000004000ULL,  /* C_31: SEND_EOT(chan_0);*/
0x0000000000003000ULL}; /* C_32: STOP(); SEND_IT();*/

