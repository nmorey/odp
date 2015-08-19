/** \brief odp_ucode_linear
 * linear transfer
 * arg0: number of 64 bits elements
 * arg1: number of 8 bits elements (payload size)
 * arg2: destination offset
 * arg3: local offset
 * arg4: packet header size (number of 64 bits elements)
 * arg5: packet header addr
 */
unsigned long long odp_ucode_linear[] __attribute__((aligned(128) )) = {
0x0000001000600000ULL,  /* C_0: dcnt0=R[0];*/
0x0000005640000000ULL,  /* C_1: ptr_1=R[2];*/
0x0000009243e80000ULL,  /* C_2: SEND_OFFSET(ptr_1,chan_0); dcnt1=R[4];*/
0x000000b400000000ULL,  /* C_3: ptr_0=R[5];*/
0x0000000803c80013ULL,  /* C_4: READ8(ptr_0,chan_0); ptr_0+=8; if(dcnt1!=0) goto C_4; dcnt1--;*/
0x000000740040001eULL,  /* C_5: ptr_0=R[3]; if(dcnt0==0) goto C_7; dcnt0--;*/
0x0000000803c0001bULL,  /* C_6: READ8(ptr_0,chan_0); ptr_0+=8; if(dcnt0!=0) goto C_6; dcnt0--;*/
0x0000003000608041ULL,  /* C_7: FLUSH(chan_0); goto C_16; dcnt0=R[1];*/
0x0000000000001000ULL,  /* C_8: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_9: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_10: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_11: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_12: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_13: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_14: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_15: STOP(); ALIGN DROP ADDRESS */
0x000000000040004aULL,  /* C_16: if(dcnt0==0) goto C_18; dcnt0--;*/
0x0000000802400047ULL,  /* C_17: READ1(ptr_0,chan_0); ptr_0+=1; if(dcnt0!=0) goto C_17; dcnt0--;*/
0x0000000000007000ULL}; /* C_18: STOP(); SEND_IT(); SEND_EOT(chan_0);*/

