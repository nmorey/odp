include $(top_srcdir)/Makefile.inc
include $(top_srcdir)/platform/Makefile.inc

AM_CFLAGS +=  -I$(srcdir)/include
AM_CFLAGS +=  -I$(top_srcdir)/include
AM_CFLAGS +=  -I$(top_srcdir)/helper/include

include_HEADERS = \
		  $(top_srcdir)/include/odp.h

odpincludedir= $(includedir)/odp
odpinclude_HEADERS = \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/align.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/atomic.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/barrier.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/buffer.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/byteorder.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/classification.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/compiler.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/config.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/cpu.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/cpumask.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/crypto.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/debug.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/errno.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/event.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/hints.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/init.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/packet_flags.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/packet.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/packet_io.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/pool.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/queue.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/random.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/rwlock.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/schedule.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/shared_memory.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/spinlock.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/std_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/sync.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/system_info.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/thread.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/ticketlock.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/time.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/timer.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/version.h

odpplatincludedir= $(includedir)/odp/plat
odpplatinclude_HEADERS = \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/atomic_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/barrier_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/buffer_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/byteorder_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/classification_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/cpumask_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/crypto_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/event_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/packet_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/packet_io_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/pool_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/queue_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/rwlock_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/schedule_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/shared_memory_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/spinlock_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/strong_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/ticketlock_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/timer_types.h \
		  $(top_srcdir)/platform/k1-nodeos/include/odp/plat/version_types.h

odpapiincludedir= $(includedir)/odp/api
odpapiinclude_HEADERS = \
		  $(top_srcdir)/include/odp/api/align.h \
		  $(top_srcdir)/include/odp/api/atomic.h \
		  $(top_srcdir)/include/odp/api/barrier.h \
		  $(top_srcdir)/include/odp/api/buffer.h \
		  $(top_srcdir)/include/odp/api/byteorder.h \
		  $(top_srcdir)/include/odp/api/classification.h \
		  $(top_srcdir)/include/odp/api/compiler.h \
		  $(top_srcdir)/include/odp/api/config.h \
		  $(top_srcdir)/include/odp/api/cpu.h \
		  $(top_srcdir)/include/odp/api/cpumask.h \
		  $(top_srcdir)/include/odp/api/crypto.h \
		  $(top_srcdir)/include/odp/api/debug.h \
		  $(top_srcdir)/include/odp/api/errno.h \
		  $(top_srcdir)/include/odp/api/event.h \
		  $(top_srcdir)/include/odp/api/hints.h \
		  $(top_srcdir)/include/odp/api/init.h \
		  $(top_srcdir)/include/odp/api/packet.h \
		  $(top_srcdir)/include/odp/api/packet_flags.h \
		  $(top_srcdir)/include/odp/api/packet_io.h \
		  $(top_srcdir)/include/odp/api/pool.h \
		  $(top_srcdir)/include/odp/api/queue.h \
		  $(top_srcdir)/include/odp/api/random.h \
		  $(top_srcdir)/include/odp/api/rwlock.h \
		  $(top_srcdir)/include/odp/api/schedule.h \
		  $(top_srcdir)/include/odp/api/shared_memory.h \
		  $(top_srcdir)/include/odp/api/spinlock.h \
		  $(top_srcdir)/include/odp/api/std_types.h \
		  $(top_srcdir)/include/odp/api/sync.h \
		  $(top_srcdir)/include/odp/api/system_info.h \
		  $(top_srcdir)/include/odp/api/thread.h \
		  $(top_srcdir)/include/odp/api/ticketlock.h \
		  $(top_srcdir)/include/odp/api/time.h \
		  $(top_srcdir)/include/odp/api/timer.h \
		  $(top_srcdir)/include/odp/api/version.h

noinst_HEADERS = \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_align_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_atomic_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_buffer_inlines.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_buffer_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_classification_datamodel.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_classification_inlines.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_classification_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_crypto_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_debug_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_packet_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_packet_io_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_packet_io_queue.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_pool_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_queue_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_schedule_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_spin_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/include/odp_timer_internal.h \
		  ${top_srcdir}/platform/k1-nodeos/Makefile.inc

subdirheadersdir = $(includedir)/odp/helper
subdirheaders_HEADERS = \
			$(top_srcdir)/helper/include/odp/helper/chksum.h \
			$(top_srcdir)/helper/include/odp/helper/eth.h \
			$(top_srcdir)/helper/include/odp/helper/icmp.h \
			$(top_srcdir)/helper/include/odp/helper/ip.h \
			$(top_srcdir)/helper/include/odp/helper/ipsec.h \
			$(top_srcdir)/helper/include/odp/helper/linux.h \
			$(top_srcdir)/helper/include/odp/helper/ring.h \
			$(top_srcdir)/helper/include/odp/helper/tcp.h \
			$(top_srcdir)/helper/include/odp/helper/udp.h

__LIB__libodp_la_SOURCES = \
			   odp_barrier.c \
			   odp_buffer.c \
			   odp_classification.c \
			   odp_cpumask.c \
			   odp_errno.c \
			   odp_event.c \
			   odp_init.c \
			   odp_impl.c \
			   odp_linux.c \
			   odp_packet.c \
			   odp_packet_flags.c \
			   odp_packet_io.c \
			   odp_packet_magic.c \
			   odp_pktio_loop.c \
			   odp_pool.c \
			   odp_queue.c \
			   ../../helper/ring.c \
			   odp_rwlock.c \
			   odp_schedule.c \
			   odp_shared_memory.c \
			   odp_spinlock.c \
			   odp_system_info.c \
			   odp_thread.c \
			   odp_ticketlock.c \
			   odp_time.c \
			   odp_timer.c \
			   odp_version.c \
			   odp_weak.c

if crypto
__LIB__libodp_la_SOURCES += odp_crypto.c 
endif

