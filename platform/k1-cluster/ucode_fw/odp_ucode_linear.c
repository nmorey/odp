/** \brief odp_ucode_linear
 * linear transfer
 * arg0: number of 64 bits elements
 * arg1: number of 8 bits elements (payload size)
 * arg2: destination offset
 * arg3: local offset
 */
unsigned long long odp_ucode_linear[] __attribute__((aligned(128) )) = {
0x0000001000600000ULL,  /* C_0: dcnt0=R[0];*/
0x0000005640000000ULL,  /* C_1: ptr_1=R[2];*/
0x0000000243800000ULL,  /* C_2: SEND_OFFSET(ptr_1,chan_0);*/
0x0000007400400016ULL,  /* C_3: ptr_0=R[3]; if(dcnt0==0) goto C_5; dcnt0--;*/
0x0000000803c00013ULL,  /* C_4: READ8(ptr_0,chan_0); ptr_0+=8; if(dcnt0!=0) goto C_4; dcnt0--;*/
0x0000003000608000ULL,  /* C_5: FLUSH(chan_0); dcnt0=R[1];*/
0x0000000000400046ULL,  /* C_6: if(dcnt0==0) goto C_17; dcnt0--;*/
0x0000000000000041ULL,  /* C_7: goto C_16;*/
0x0000000000001000ULL,  /* C_8: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_9: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_10: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_11: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_12: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_13: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_14: STOP(); ALIGN DROP ADDRESS */
0x0000000000001000ULL,  /* C_15: STOP(); ALIGN DROP ADDRESS */
0x0000000802400043ULL,  /* C_16: READ1(ptr_0,chan_0); ptr_0+=1; if(dcnt0!=0) goto C_16; dcnt0--;*/
0x0000000000007000ULL}; /* C_17: STOP(); SEND_IT(); SEND_EOT(chan_0);*/

