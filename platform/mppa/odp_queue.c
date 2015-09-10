/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/queue.h>
#include <odp_queue_internal.h>
#include <odp/std_types.h>
#include <odp/align.h>
#include <odp/buffer.h>
#include <odp_buffer_internal.h>
#include <odp_pool_internal.h>
#include <odp_buffer_inlines.h>
#include <odp_internal.h>
#include <odp/shared_memory.h>
#include <odp/schedule.h>
#include <odp_schedule_internal.h>
#include <odp/config.h>
#include <odp_packet_io_internal.h>
#include <odp_packet_io_queue.h>
#include <odp_debug_internal.h>
#include <odp/hints.h>
#include <odp/sync.h>
#include <odp_spin_internal.h>

#ifdef USE_TICKETLOCK
#include <odp/ticketlock.h>
#define LOCK(a)      do {				\
		__k1_wmb();				\
		odp_ticketlock_lock(&(a)->s.lock);	\
	} while(0)

#define UNLOCK(a)    do {				\
		__k1_wmb();				\
		odp_ticketlock_unlock(&(a)->s.lock);	\
	}while(0)

#define LOCK_INIT(a) odp_ticketlock_init(&(a)->s.lock)
#define LOCK_TRY(a)  ({ __k1_wmb(); odp_ticketlock_trylock(&(a)->s.lock); })

#else
#include <odp/spinlock.h>
#define LOCK(a)      do {				\
		INVALIDATE(queue);			\
		odp_spinlock_lock(&(a)->s.lock);	\
	} while(0)
#define UNLOCK(a)    do {				\
		__k1_wmb();				\
		odp_spinlock_unlock(&(a)->s.lock);	\
	}while(0)
#define LOCK_INIT(a) odp_spinlock_init(&(a)->s.lock)
#define LOCK_TRY(a)  ({ __k1_wmb(); odp_spinlock_trylock(&(a)->s.lock); })
#endif

#include <string.h>

typedef struct queue_table_t {
	queue_entry_t  queue[ODP_CONFIG_QUEUES];
} queue_table_t;

static queue_table_t queue_tbl;

static void queue_init(queue_entry_t *queue, const char *name,
		       odp_queue_type_t type, odp_queue_param_t *param)
{
	strncpy(queue->s.name, name, ODP_QUEUE_NAME_LEN - 1);
	queue->s.type = type;

	if (param) {
		memcpy(&queue->s.param, param, sizeof(odp_queue_param_t));
	} else {
		/* Defaults */
		memset(&queue->s.param, 0, sizeof(odp_queue_param_t));
		queue->s.param.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
		queue->s.param.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
		queue->s.param.sched.group = ODP_SCHED_GROUP_ALL;
	}

	switch (type) {
	case ODP_QUEUE_TYPE_PKTIN:
		queue->s.enqueue = pktin_enqueue;
		queue->s.dequeue = pktin_dequeue;
		queue->s.enqueue_multi = pktin_enq_multi;
		queue->s.dequeue_multi = pktin_deq_multi;
		break;
	case ODP_QUEUE_TYPE_PKTOUT:
		queue->s.enqueue = queue_pktout_enq;
		queue->s.dequeue = pktout_dequeue;
		queue->s.enqueue_multi = queue_pktout_enq_multi;
		queue->s.dequeue_multi = pktout_deq_multi;
		break;
	default:
		queue->s.enqueue = queue_enq;
		queue->s.dequeue = queue_deq;
		queue->s.enqueue_multi = queue_enq_multi;
		queue->s.dequeue_multi = queue_deq_multi;
		break;
	}

	queue->s.head = NULL;
	queue->s.tail = NULL;

	queue->s.reorder_head = NULL;
	queue->s.reorder_tail = NULL;

	queue->s.pri_queue = ODP_QUEUE_INVALID;
	queue->s.cmd_ev    = ODP_EVENT_INVALID;
}

uint32_t queue_to_id(odp_queue_t handle)
{
	queue_entry_t * qe = queue_to_qentry(handle);
	return qe - &queue_tbl.queue[0];
}
int odp_queue_init_global(void)
{
	uint32_t i;

	ODP_DBG("Queue init ... ");

	memset(&queue_tbl, 0, sizeof(queue_table_t));

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		/* init locks */
		queue_entry_t *queue = &queue_tbl.queue[i];
		LOCK_INIT(queue);
		odp_atomic_init_u64(&queue->s.sync_in, 0);
		odp_atomic_init_u64(&queue->s.sync_out, 0);
	}

	ODP_DBG("done\n");
	ODP_DBG("Queue init global\n");
	ODP_DBG("  struct queue_entry_s size %zu\n",
		sizeof(struct queue_entry_s));
	ODP_DBG("  queue_entry_t size        %zu\n",
		sizeof(queue_entry_t));
	ODP_DBG("\n");
	__k1_wmb();
	return 0;
}

int odp_queue_term_global(void)
{
	int rc = 0;
	queue_entry_t *queue;
	int i;

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		queue = &queue_tbl.queue[i];
		LOCK(queue);
		if (LOAD_S32(queue->s.status) != QUEUE_STATUS_FREE) {

			ODP_ERR("Not destroyed queue: %s\n", queue->s.name);
			rc = -1;
		}
		UNLOCK(queue);
	}

	return rc;
}

odp_queue_type_t odp_queue_type(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.type;
}

odp_schedule_sync_t odp_queue_sched_type(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.param.sched.sync;
}

odp_schedule_prio_t odp_queue_sched_prio(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.param.sched.prio;
}

odp_schedule_group_t odp_queue_sched_group(odp_queue_t handle)
{
	queue_entry_t *queue;

	queue = queue_to_qentry(handle);

	return queue->s.param.sched.group;
}

odp_queue_t odp_queue_create(const char *name, odp_queue_type_t type,
			     odp_queue_param_t *param)
{
	uint32_t i;
	queue_entry_t *queue;
	odp_queue_t handle = ODP_QUEUE_INVALID;
	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		queue = &queue_tbl.queue[i];

		if (LOAD_S32(queue->s.status) != QUEUE_STATUS_FREE)
			continue;

		LOCK(queue);
		INVALIDATE(queue);
		if (queue->s.status == QUEUE_STATUS_FREE) {
			queue_init(queue, name, type, param);

			if (type == ODP_QUEUE_TYPE_SCHED ||
			    type == ODP_QUEUE_TYPE_PKTIN)
				queue->s.status = QUEUE_STATUS_NOTSCHED;
			else
				queue->s.status = QUEUE_STATUS_READY;

			handle = queue_handle(queue);
			UNLOCK(queue);
			break;
		}
		UNLOCK(queue);
	}

	if (handle != ODP_QUEUE_INVALID &&
	    (type == ODP_QUEUE_TYPE_SCHED || type == ODP_QUEUE_TYPE_PKTIN)) {
		if (schedule_queue_init(queue)) {
			ODP_ERR("schedule queue init failed\n");
			return ODP_QUEUE_INVALID;
		}
	}

	return handle;
}

void queue_destroy_finalize(queue_entry_t *queue)
{
	LOCK(queue);

	if (LOAD_S32(queue->s.status) == QUEUE_STATUS_DESTROYED) {
		INVALIDATE(queue);
		queue->s.status = QUEUE_STATUS_FREE;
		schedule_queue_destroy(queue);
	}
	UNLOCK(queue);
}

int odp_queue_destroy(odp_queue_t handle)
{
	queue_entry_t *queue;
	queue = queue_to_qentry(handle);

	LOCK(queue);
	INVALIDATE(queue);
	if (queue->s.status == QUEUE_STATUS_FREE) {
		UNLOCK(queue);
		ODP_ERR("queue \"%s\" already free\n", queue->s.name);
		return -1;
	}
	if (queue->s.status == QUEUE_STATUS_DESTROYED) {
		UNLOCK(queue);
		ODP_ERR("queue \"%s\" already destroyed\n", queue->s.name);
		return -1;
	}
	if (queue->s.head != NULL) {
		UNLOCK(queue);
		ODP_ERR("queue \"%s\" not empty\n", queue->s.name);
		return -1;
	}
	if (queue_is_ordered(queue) && queue->s.reorder_head) {
		UNLOCK(queue);
		ODP_ERR("queue \"%s\" reorder queue not empty\n",
			queue->s.name);
		return -1;
	}

	switch (queue->s.status) {
	case QUEUE_STATUS_READY:
		queue->s.status = QUEUE_STATUS_FREE;
		break;
	case QUEUE_STATUS_NOTSCHED:
		queue->s.status = QUEUE_STATUS_FREE;
		schedule_queue_destroy(queue);
		break;
	case QUEUE_STATUS_SCHED:
		/* Queue is still in scheduling */
		queue->s.status = QUEUE_STATUS_DESTROYED;
		break;
	default:
		ODP_ABORT("Unexpected queue status\n");
	}
	UNLOCK(queue);

	return 0;
}

int odp_queue_context_set(odp_queue_t handle, void *context)
{
	queue_entry_t *queue;
	queue = queue_to_qentry(handle);
	odp_sync_stores();
	queue->s.param.context = context;
	odp_sync_stores();
	return 0;
}

void *odp_queue_context(odp_queue_t handle)
{
	queue_entry_t *queue;
	queue = queue_to_qentry(handle);
	return queue->s.param.context;
}

odp_queue_t odp_queue_lookup(const char *name)
{
	uint32_t i;

	for (i = 0; i < ODP_CONFIG_QUEUES; i++) {
		queue_entry_t *queue = &queue_tbl.queue[i];

		if (queue->s.status == QUEUE_STATUS_FREE ||
		    queue->s.status == QUEUE_STATUS_DESTROYED)
			continue;

		LOCK(queue);
		if (strcmp(name, queue->s.name) == 0) {
			/* found it */
			UNLOCK(queue);
			return queue_handle(queue);
		}
		UNLOCK(queue);
	}

	return ODP_QUEUE_INVALID;
}

/* Update queue head and/or tail and schedule status
 * Return if the queue needs to be reschedule.
 * Queue must be locked before calling this function
 */
static int _queue_enq_update(queue_entry_t *queue, odp_buffer_hdr_t *head,
			     odp_buffer_hdr_t *tail, int status){
	if (LOAD_PTR(queue->s.head) == NULL) {
		/* Empty queue */
		STORE_PTR(queue->s.head, head);
		STORE_PTR(queue->s.tail, tail);
		tail->next = NULL;
	} else {
		STORE_PTR(((typeof(queue->s.tail))LOAD_PTR(queue->s.tail))->next, head);
		STORE_PTR(queue->s.tail, tail);
		tail->next = NULL;
	}

	if (status == QUEUE_STATUS_NOTSCHED) {
		STORE_S32(queue->s.status, QUEUE_STATUS_SCHED);
		return 1; /* retval: schedule queue */
	}
	return 0;
}

static int _queue_enq_ordered(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr,
			      int sustain, uint64_t order,
			      queue_entry_t *origin_qe)
{
	int sched = 0;
	odp_buffer_hdr_t *buf_tail;

	LOCK(origin_qe);

	/* Need two locks for enq operations from ordered queues */
	while (!LOCK_TRY(queue)) {
		UNLOCK(origin_qe);
		LOCK(origin_qe);
	}

	if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(queue);
		UNLOCK(origin_qe);
		ODP_ERR("Bad origin queue status\n");
		ODP_ERR("queue = %s, origin q = %s, buf = %p\n",
			queue->s.name, origin_qe->s.name, buf_hdr);
		return -1;
	}

	int status = LOAD_S32(queue->s.status);
	if (odp_unlikely(status < QUEUE_STATUS_READY)) {
		UNLOCK(queue);
		UNLOCK(origin_qe);
		ODP_ERR("Bad queue status\n");
		return -1;
	}

	/* We can only complete the enq if we're in order */
	sched_enq_called();
	if (order > origin_qe->s.order_out) {
		reorder_enq(queue, order, origin_qe, buf_hdr, sustain);

		/* This enq can't complete until order is restored, so
		 * we're done here.
		 */
		UNLOCK(queue);
		UNLOCK(origin_qe);
		return 0;
	}

	/* We're in order, so account for this and proceed with enq */
	if (!sustain) {
		order_release(origin_qe, 1);
		sched_order_resolved(buf_hdr);
	}

	/* if this element is linked, restore the linked chain */
	buf_tail = buf_hdr->link;

	if (buf_tail) {
		buf_hdr->next = buf_tail;
		buf_hdr->link = NULL;

		/* find end of the chain */
		while (buf_tail->next)
			buf_tail = buf_tail->next;
	} else {
		buf_tail = buf_hdr;
	}

	sched = _queue_enq_update(queue, buf_hdr, buf_tail, status);

	/*
	 * If we came from an ordered queue, check to see if our successful
	 * enq has unblocked other buffers in the origin's reorder queue.
	 */
	odp_buffer_hdr_t *reorder_buf;
	odp_buffer_hdr_t *next_buf;
	odp_buffer_hdr_t *reorder_prev;
	odp_buffer_hdr_t *placeholder_buf;
	int               deq_count, release_count, placeholder_count;

	deq_count = reorder_deq(queue, origin_qe, &reorder_buf,
				&reorder_prev, &placeholder_buf,
				&release_count, &placeholder_count);

	/* Add released buffers to the queue as well */
	if (deq_count > 0) {
		queue->s.tail->next       = origin_qe->s.reorder_head;
		queue->s.tail             = reorder_prev;
		origin_qe->s.reorder_head = reorder_prev->next;
		reorder_prev->next        = NULL;
	}

	/* Reflect resolved orders in the output sequence */
	order_release(origin_qe, release_count + placeholder_count);

	/* Now handle any unblocked complete buffers destined for
	 * other queues, appending placeholder bufs as needed.
	 */
	UNLOCK(queue);
	reorder_complete(origin_qe, &reorder_buf, &placeholder_buf, 1);
	UNLOCK(origin_qe);

	if (reorder_buf)
		queue_enq_internal(reorder_buf);

	/* Free all placeholder bufs that are now released */
	while (placeholder_buf) {
		next_buf = placeholder_buf->next;
		odp_buffer_free((odp_buffer_t)placeholder_buf);
		placeholder_buf = next_buf;
	}

	/* Add queue to scheduling */
	if (sched && schedule_queue(queue))
		ODP_ABORT("schedule_queue failed\n");

	return 0;
}

int queue_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr, int sustain)
{
	int sched = 0;
	queue_entry_t *origin_qe;
	uint64_t order;

	get_queue_order(&origin_qe, &order, buf_hdr);

	if (origin_qe)
		return _queue_enq_ordered(queue, buf_hdr, sustain,
					  order, origin_qe);

	LOCK(queue);
	int status = LOAD_S32(queue->s.status);
	if (odp_unlikely(status < QUEUE_STATUS_READY)) {
		UNLOCK(queue);
		ODP_ERR("Bad queue status\n");
		return -1;
	}

	sched = _queue_enq_update(queue, buf_hdr, buf_hdr, status);

	UNLOCK(queue);

	/* Add queue to scheduling */
	if (sched && schedule_queue(queue))
		ODP_ABORT("schedule_queue failed\n");

	return 0;
}

int queue_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[],
		    int num, int sustain)
{
	int sched = 0;
	int i, rc;
	odp_buffer_hdr_t *tail;
	queue_entry_t *origin_qe;
	uint64_t order;

	for (i = 0; i < num - 1; i++)
		buf_hdr[i]->next = buf_hdr[i+1];

	tail = buf_hdr[num-1];
	buf_hdr[num-1]->next = NULL;

	/* Handle ordered enqueues commonly via links */
	get_queue_order(&origin_qe, &order, buf_hdr[0]);
	if (origin_qe) {
		buf_hdr[0]->link = buf_hdr[0]->next;
		rc = queue_enq(queue, buf_hdr[0], sustain);
		return rc == 0 ? num : rc;
	}

	LOCK(queue);
	int status = LOAD_S32(queue->s.status);
	if (odp_unlikely(status < QUEUE_STATUS_READY)) {
		UNLOCK(queue);
		ODP_ERR("Bad queue status\n");
		return -1;
	}

	/* Empty queue */
	if (LOAD_PTR(queue->s.head) == NULL)
		STORE_PTR(queue->s.head, buf_hdr[0]);
	else
		STORE_PTR(((typeof(queue->s.tail))LOAD_PTR(queue->s.tail))->next, buf_hdr[0]);

	STORE_PTR(queue->s.tail, tail);

	if (status == QUEUE_STATUS_NOTSCHED) {
		STORE_PTR(queue->s.status, QUEUE_STATUS_SCHED);
		sched = 1; /* retval: schedule queue */
	}
	UNLOCK(queue);

	/* Add queue to scheduling */
	if (sched && schedule_queue(queue))
		ODP_ABORT("schedule_queue failed\n");

	return num; /* All events enqueued */
}

int odp_queue_enq_multi(odp_queue_t handle, const odp_event_t ev[], int num)
{
	queue_entry_t *queue;

	if (num > QUEUE_MULTI_MAX)
		num = QUEUE_MULTI_MAX;

	queue = queue_to_qentry(handle);

	return queue->s.enqueue_multi(queue, (odp_buffer_hdr_t **)ev, num, 1);
}


int odp_queue_enq(odp_queue_t handle, odp_event_t ev)
{
	queue_entry_t *queue;
	queue   = queue_to_qentry(handle);
	return queue->s.enqueue(queue, (odp_buffer_hdr_t *)ev, 1);
}

int queue_enq_internal(odp_buffer_hdr_t *buf_hdr)
{
	return buf_hdr->origin_qe->s.enqueue(buf_hdr->target_qe, buf_hdr,
					     buf_hdr->flags.sustain);
}

odp_buffer_hdr_t *queue_deq(queue_entry_t *queue)
{
	odp_buffer_hdr_t *buf_hdr;

	if (LOAD_PTR(queue->s.head) == NULL)
		return NULL;

	LOCK(queue);

	buf_hdr       = LOAD_PTR(queue->s.head);
	if (buf_hdr == NULL) {
		UNLOCK(queue);
		return NULL;
	}

	INVALIDATE(buf_hdr);
	STORE_PTR(queue->s.head, buf_hdr->next);
	if (buf_hdr->next == NULL) {
		/* Queue is now empty */
		STORE_PTR(queue->s.tail, NULL);
		if (LOAD_S32(queue->s.status) == QUEUE_STATUS_SCHED)
			STORE_S32(queue->s.status, QUEUE_STATUS_NOTSCHED);
	}

	buf_hdr->next = NULL;

	/* Note that order should really be assigned on enq to an
	 * ordered queue rather than deq, however the logic is simpler
	 * to do it here and has the same effect.
	 */
	if (queue_is_ordered(queue)) {
		buf_hdr->origin_qe = queue;
		buf_hdr->order = queue->s.order_in++;
		buf_hdr->sync  = odp_atomic_fetch_inc_u64(&queue->s.sync_in);
		buf_hdr->flags.sustain = 0;
	} else {
		buf_hdr->origin_qe = NULL;
	}

	UNLOCK(queue);

	return buf_hdr;
}


int queue_deq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num)
{
	odp_buffer_hdr_t *hdr;
	int i;

	LOCK(queue);
	int status = LOAD_S32(queue->s.status);
	if (odp_unlikely(status < QUEUE_STATUS_READY)) {
		/* Bad queue, or queue has been destroyed.
		 * Scheduler finalizes queue destroy after this. */
		UNLOCK(queue);
		return -1;
	}

	hdr = LOAD_PTR(queue->s.head);

	if (hdr == NULL) {
		/* Already empty queue */
		if (status == QUEUE_STATUS_SCHED)
			STORE_S32(queue->s.status, QUEUE_STATUS_NOTSCHED);

		UNLOCK(queue);
		return 0;
	}

	for (i = 0; i < num && hdr; i++) {
		INVALIDATE(hdr);
		buf_hdr[i]       = hdr;
		hdr              = hdr->next;
		buf_hdr[i]->next = NULL;
		if (queue_is_ordered(queue)) {
			buf_hdr[i]->origin_qe = queue;
			buf_hdr[i]->order     = queue->s.order_in++;
			buf_hdr[i]->sync =
				odp_atomic_fetch_inc_u64(&queue->s.sync_in);
			buf_hdr[i]->flags.sustain = 0;
		} else {
			buf_hdr[i]->origin_qe = NULL;
		}
	}

	STORE_PTR(queue->s.head, hdr);

	if (hdr == NULL) {
		/* Queue is now empty */
		STORE_PTR(queue->s.tail, NULL);
	}

	UNLOCK(queue);

	return i;
}

int odp_queue_deq_multi(odp_queue_t handle, odp_event_t events[], int num)
{
	queue_entry_t *queue;
	int ret;

	if (num > QUEUE_MULTI_MAX)
		num = QUEUE_MULTI_MAX;

	queue = queue_to_qentry(handle);

	ret = queue->s.dequeue_multi(queue, (odp_buffer_hdr_t **)events, num);

	return ret;
}


odp_event_t odp_queue_deq(odp_queue_t handle)
{
	queue_entry_t *queue;
	odp_buffer_hdr_t *buf_hdr;

	queue   = queue_to_qentry(handle);
	buf_hdr = queue->s.dequeue(queue);

	if (buf_hdr)
		return (odp_event_t)buf_hdr;

	return ODP_EVENT_INVALID;
}

int queue_pktout_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr,
		     int sustain)
{
	queue_entry_t *origin_qe;
	uint64_t order;
	int rc;

	/* Special processing needed only if we came from an ordered queue */
	get_queue_order(&origin_qe, &order, buf_hdr);
	if (!origin_qe)
		return pktout_enqueue(queue, buf_hdr);

	/* Must lock origin_qe for ordered processing */
	LOCK(origin_qe);
	if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(origin_qe);
		ODP_ERR("Bad origin queue status\n");
		return -1;
	}

	/* We can only complete the enq if we're in order */
	sched_enq_called();
	if (order > origin_qe->s.order_out) {
		reorder_enq(queue, order, origin_qe, buf_hdr, sustain);

		/* This enq can't complete until order is restored, so
		 * we're done here.
		 */
		UNLOCK(origin_qe);
		return 0;
	}

	/* Perform our enq since we're in order.
	 * Note: Don't hold the origin_qe lock across an I/O operation!
	 */
	UNLOCK(origin_qe);

	/* Handle any chained buffers (internal calls) */
	if (buf_hdr->link) {
		odp_buffer_hdr_t *buf_hdrs[QUEUE_MULTI_MAX];
		odp_buffer_hdr_t *next_buf;
		int num = 0;

		next_buf = buf_hdr->link;
		buf_hdr->link = NULL;

		while (next_buf) {
			buf_hdrs[num++] = next_buf;
			next_buf = next_buf->next;
		}

		rc = pktout_enq_multi(queue, buf_hdrs, num);
		if (rc < num)
			return -1;
	} else {
		rc = pktout_enqueue(queue, buf_hdr);
		if (!rc)
			return rc;
	}

	/* Reacquire the lock following the I/O send. Note that we're still
	 * guaranteed to be in order here since we haven't released
	 * order yet.
	 */
	LOCK(origin_qe);
	if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
		UNLOCK(origin_qe);
		ODP_ERR("Bad origin queue status\n");
		return -1;
	}

	/* Account for this ordered enq */
	if (!sustain) {
		order_release(origin_qe, 1);
		sched_order_resolved(NULL);
	}

	/* Now check to see if our successful enq has unblocked other buffers
	 * in the origin's reorder queue.
	 */
	odp_buffer_hdr_t *reorder_buf;
	odp_buffer_hdr_t *next_buf;
	odp_buffer_hdr_t *reorder_prev;
	odp_buffer_hdr_t *xmit_buf;
	odp_buffer_hdr_t *placeholder_buf;
	int               deq_count, release_count, placeholder_count;

	deq_count = reorder_deq(queue, origin_qe,
				&reorder_buf, &reorder_prev, &placeholder_buf,
				&release_count, &placeholder_count);

	/* Send released buffers as well */
	if (deq_count > 0) {
		xmit_buf = origin_qe->s.reorder_head;
		origin_qe->s.reorder_head = reorder_prev->next;
		reorder_prev->next = NULL;
		UNLOCK(origin_qe);

		do {
			next_buf = xmit_buf->next;
			pktout_enqueue(queue, xmit_buf);
			xmit_buf = next_buf;
		} while (xmit_buf);

		/* Reacquire the origin_qe lock to continue */
		LOCK(origin_qe);
		if (odp_unlikely(origin_qe->s.status < QUEUE_STATUS_READY)) {
			UNLOCK(origin_qe);
			ODP_ERR("Bad origin queue status\n");
			return -1;
		}
	}

	/* Update the order sequence to reflect the deq'd elements */
	order_release(origin_qe, release_count + placeholder_count);

	/* Now handle sends to other queues that are ready to go */
	reorder_complete(origin_qe, &reorder_buf, &placeholder_buf, 1);

	/* We're fully done with the origin_qe at last */
	UNLOCK(origin_qe);

	/* Now send the next buffer to its target queue */
	if (reorder_buf)
		queue_enq_internal(reorder_buf);

	/* Free all placeholder bufs that are now released */
	while (placeholder_buf) {
		next_buf = placeholder_buf->next;
		odp_buffer_free((odp_buffer_t)placeholder_buf);
		placeholder_buf = next_buf;
	}

	return 0;
}

int queue_pktout_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[],
			   int num, int sustain)
{
	int i, rc;
	queue_entry_t *origin_qe;
	uint64_t order;

	/* If we're not ordered, handle directly */
	get_queue_order(&origin_qe, &order, buf_hdr[0]);
	if (!origin_qe)
		return pktout_enq_multi(queue, buf_hdr, num);

	/* Chain input buffers together */
	for (i = 0; i < num - 1; i++)
		buf_hdr[i]->next = buf_hdr[i + 1];

	buf_hdr[num - 1]->next = NULL;

	/* Handle commonly via links */
	buf_hdr[0]->link = buf_hdr[0]->next;
	rc = queue_pktout_enq(queue, buf_hdr[0], sustain);
	return rc == 0 ? num : rc;
}

void queue_lock(queue_entry_t *queue)
{
	LOCK(queue);
}

void queue_unlock(queue_entry_t *queue)
{
	UNLOCK(queue);
}

void odp_queue_param_init(odp_queue_param_t *params)
{
	memset(params, 0, sizeof(odp_queue_param_t));
}

/* These routines exists here rather than in odp_schedule
 * because they operate on queue interenal structures
 */
int release_order(queue_entry_t *origin_qe, uint64_t order,
		  odp_pool_t pool, int enq_called)
{
	odp_buffer_t placeholder_buf;
	odp_buffer_hdr_t *placeholder_buf_hdr, *reorder_buf, *next_buf;

	/* Must tlock the origin queue to process the release */
	LOCK(origin_qe);

	/* If we are in the order we can release immediately since there can
	 * be no confusion about intermediate elements
	 */
	if (order <= origin_qe->s.order_out) {
		order_release(origin_qe, 1);

		/* Check if this release allows us to unblock waiters.
		 * At the point of this call, the reorder list may contain
		 * zero or more placeholders that need to be freed, followed
		 * by zero or one complete reorder buffer chain.
		 */
		reorder_complete(origin_qe, &reorder_buf,
				 &placeholder_buf_hdr, 0);

		/* Now safe to unlock */
		UNLOCK(origin_qe);

		/* If reorder_buf has a target, do the enq now */
		if (reorder_buf)
			queue_enq_internal(reorder_buf);

		while (placeholder_buf_hdr) {
			odp_buffer_hdr_t *placeholder_next =
				placeholder_buf_hdr->next;

			odp_buffer_free((odp_buffer_t)placeholder_buf_hdr);
			placeholder_buf_hdr = placeholder_next;
		}

		return 0;
	}

	/* If we are not in order we need a placeholder to represent our
	 * "place in line" unless we have issued enqs, in which case we
	 * already have a place in the reorder queue. If we need a
	 * placeholder, use an element from the same pool we were scheduled
	 * with is from, otherwise just ensure that the final element for our
	 * order is not marked sustain.
	 */
	if (enq_called) {
		reorder_buf = NULL;
		next_buf    = origin_qe->s.reorder_head;

		while (next_buf && next_buf->order <= order) {
			reorder_buf = next_buf;
			next_buf = next_buf->next;
		}

		if (reorder_buf && reorder_buf->order == order) {
			reorder_buf->flags.sustain = 0;
			UNLOCK(origin_qe);
			return 0;
		}
	}

	placeholder_buf = odp_buffer_alloc(pool);

	/* Can't release if no placeholder is available */
	if (odp_unlikely(placeholder_buf == ODP_BUFFER_INVALID)) {
		UNLOCK(origin_qe);
		return -1;
	}

	placeholder_buf_hdr = odp_buf_to_hdr(placeholder_buf);

	/* Copy info to placeholder and add it to the reorder queue */
	placeholder_buf_hdr->origin_qe     = origin_qe;
	placeholder_buf_hdr->order         = order;
	placeholder_buf_hdr->flags.sustain = 0;

	reorder_enq(NULL, order, origin_qe, placeholder_buf_hdr, 0);

	UNLOCK(origin_qe);
	return 0;
}

/* This routine is a no-op in linux-generic */
int odp_schedule_order_lock_init(odp_schedule_order_lock_t *lock ODP_UNUSED,
				 odp_queue_t queue ODP_UNUSED)
{
	return 0;
}

void odp_schedule_order_lock(odp_schedule_order_lock_t *lock ODP_UNUSED)
{
	queue_entry_t *origin_qe;
	uint64_t *sync;

	get_sched_sync(&origin_qe, &sync);
	if (!origin_qe)
		return;

	/* Wait until we are in order. Note that sync_out will be incremented
	 * both by unlocks as well as order resolution, so we're OK if only
	 * some events in the ordered flow need to lock.
	 */
	while (*sync > odp_atomic_load_u64(&origin_qe->s.sync_out))
		odp_spin();
}

void odp_schedule_order_unlock(odp_schedule_order_lock_t *lock ODP_UNUSED)
{
	queue_entry_t *origin_qe;
	uint64_t *sync;

	get_sched_sync(&origin_qe, &sync);
	if (!origin_qe)
		return;

	/* Release the ordered lock */
	odp_atomic_fetch_inc_u64(&origin_qe->s.sync_out);
}
