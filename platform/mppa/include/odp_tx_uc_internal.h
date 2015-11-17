/**
 * @file
 *
 * ODP Rx thread - implementation internal
 */

#ifndef ODP_TX_UC_INTERNAL_H_
#define ODP_TX_UC_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_packet_io_internal.h>
#include <mppa_noc.h>

#define NOC_UC_COUNT		2
#define MAX_PKT_PER_UC		4
#define MAX_JOB_PER_UC          MOS_NB_UC_TRS
#define DNOC_CLUS_IFACE_ID	0

typedef struct eth_uc_job_ctx {
	odp_packet_t pkt_table[MAX_PKT_PER_UC];
	unsigned int pkt_count;
	unsigned char nofree;
} tx_uc_job_ctx_t;

typedef struct {
	unsigned int dnoc_tx_id;
	unsigned int dnoc_uc_id;
	odp_atomic_u64_t head;
#if MOS_UC_VERSION == 1
	odp_atomic_u64_t commit_head;
#endif
	tx_uc_job_ctx_t job_ctxs[MAX_JOB_PER_UC];
} tx_uc_ctx_t;

int tx_uc_init(void);
uint64_t tx_uc_alloc_uc_slots(tx_uc_ctx_t *ctx,
			      unsigned int count);

void tx_uc_commit(tx_uc_ctx_t *ctx, uint64_t slot,
		  unsigned int count);
int tx_uc_send_packets(const pkt_tx_uc_config *tx_config,
		       tx_uc_ctx_t *ctx, odp_packet_t pkt_table[],
		       int pkt_count, int mtu, int *err);
void tx_uc_flush(tx_uc_ctx_t *ctx);

extern tx_uc_ctx_t g_tx_uc_ctx[NOC_UC_COUNT];
#ifdef __cplusplus
}
#endif

#endif
