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
#include <odp_schedule_internal.h>
#include <odp/config.h>
#include <odp_packet_io_internal.h>
#include <odp_packet_io_queue.h>
#include <odp_debug_internal.h>
#include <odp/hints.h>
#include <odp/sync.h>

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
		queue->s.param.sched.sync  = ODP_SCHED_SYNC_DEFAULT;
		queue->s.param.sched.group = ODP_SCHED_GROUP_DEFAULT;
	}

	switch (type) {
	case ODP_QUEUE_TYPE_PKTIN:
		queue->s.enqueue = pktin_enqueue;
		queue->s.dequeue = pktin_dequeue;
		queue->s.enqueue_multi = pktin_enq_multi;
		queue->s.dequeue_multi = pktin_deq_multi;
		break;
	case ODP_QUEUE_TYPE_PKTOUT:
		queue->s.enqueue = pktout_enqueue;
		queue->s.dequeue = pktout_dequeue;
		queue->s.enqueue_multi = pktout_enq_multi;
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

int odp_queue_set_context(odp_queue_t handle, void *context)
{
	queue_entry_t *queue;
	queue = queue_to_qentry(handle);
	odp_sync_stores();
	queue->s.param.context = context;
	odp_sync_stores();
	return 0;
}

void *odp_queue_get_context(odp_queue_t handle)
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

		if (queue->s.status == QUEUE_STATUS_FREE)
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


int queue_enq(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr)
{
	int sched = 0;

	LOCK(queue);
	int status = LOAD_S32(queue->s.status);
	if (odp_unlikely(status < QUEUE_STATUS_READY)) {
		UNLOCK(queue);
		ODP_ERR("Bad queue status\n");
		return -1;
	}

	if (LOAD_PTR(queue->s.head) == NULL) {
		/* Empty queue */
		STORE_PTR(queue->s.head, buf_hdr);
		STORE_PTR(queue->s.tail, buf_hdr);
		buf_hdr->next = NULL;
	} else {
		STORE_PTR(((typeof(queue->s.tail))LOAD_PTR(queue->s.tail))->next, buf_hdr);
		STORE_PTR(queue->s.tail, buf_hdr);
		buf_hdr->next = NULL;
	}

	if (status == QUEUE_STATUS_NOTSCHED) {
		STORE_S32(queue->s.status, QUEUE_STATUS_SCHED);
		sched = 1; /* retval: schedule queue */
	}
	UNLOCK(queue);

	/* Add queue to scheduling */
	if (sched)
		schedule_queue(queue);

	return 0;
}

int queue_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num)
{
	int sched = 0;
	int i;
	odp_buffer_hdr_t *tail;

	for (i = 0; i < num - 1; i++)
		buf_hdr[i]->next = buf_hdr[i+1];

	tail = buf_hdr[num-1];
	buf_hdr[num-1]->next = NULL;

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
	odp_buffer_hdr_t *buf_hdr[QUEUE_MULTI_MAX];
	queue_entry_t *queue;
	int i;

	if (num > QUEUE_MULTI_MAX)
		num = QUEUE_MULTI_MAX;

	queue = queue_to_qentry(handle);

	for (i = 0; i < num; i++)
		buf_hdr[i] = odp_buf_to_hdr(odp_buffer_from_event(ev[i]));

	return queue->s.enqueue_multi(queue, buf_hdr, num);
}


int odp_queue_enq(odp_queue_t handle, odp_event_t ev)
{
	odp_buffer_hdr_t *buf_hdr;
	queue_entry_t *queue;

	queue   = queue_to_qentry(handle);
	buf_hdr = odp_buf_to_hdr(odp_buffer_from_event(ev));

	return queue->s.enqueue(queue, buf_hdr);
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
	odp_buffer_hdr_t *buf_hdr[QUEUE_MULTI_MAX];
	int i, ret;

	if (num > QUEUE_MULTI_MAX)
		num = QUEUE_MULTI_MAX;

	queue = queue_to_qentry(handle);

	ret = queue->s.dequeue_multi(queue, buf_hdr, num);

	for (i = 0; i < ret; i++)
		events[i] = odp_buffer_to_event((odp_buffer_t)buf_hdr[i]);

	return ret;
}


odp_event_t odp_queue_deq(odp_queue_t handle)
{
	queue_entry_t *queue;
	odp_buffer_hdr_t *buf_hdr;

	queue   = queue_to_qentry(handle);
	buf_hdr = queue->s.dequeue(queue);

	if (buf_hdr)
		return odp_buffer_to_event((odp_buffer_t)buf_hdr);

	return ODP_EVENT_INVALID;
}


void queue_lock(queue_entry_t *queue)
{
	LOCK(queue);
}


void queue_unlock(queue_entry_t *queue)
{
	UNLOCK(queue);
}
