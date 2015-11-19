AM_CONDITIONAL([netmap_support], [test x$netmap_support = xyes ])
AM_CONDITIONAL([PKTIO_IPC], [test x$pktio_ipc_support = xyes])
AM_CONDITIONAL([PKTIO_DPDK], [test x$pktio_dpdk_support = xyes ])
AM_CONDITIONAL([HAVE_PCAP], [test $have_pcap = yes])
