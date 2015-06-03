include $(top_srcdir)/Makefile.inc
include $(top_srcdir)/platform/@with_platform@/Makefile.inc
LIB   = $(top_builddir)/lib
LDADD = $(LIB)/libodp.la
INCFLAGS = -I$(srcdir) \
	-I$(top_srcdir)/test \
	-I$(top_srcdir)/platform/@with_platform@/include \
	-I$(top_srcdir)/platform/linux-generic/include \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/helper/include
AM_CFLAGS += $(INCFLAGS)
AM_CXXFLAGS = $(INCFLAGS)

AM_LDFLAGS += -L$(LIB)

@VALGRIND_CHECK_RULES@
valgrind_tools = memcheck drd sgcheck

TESTS_ENVIRONMENT = ODP_PLATFORM=${with_platform} TEST_DIR="${builddir}" TARGET_RUNNER="$(KELF_LOG_COMPILER)"
TEST_EXTENSIONS = .kelf

if TARGET_IS_K1_JTAG
KELF_LOG_COMPILER =	$(top_srcdir)/ktest-wrapper.sh
endif

if TARGET_IS_K1_CLUSTER
KELF_LOG_COMPILER =	k1-cluster   --functional  --mboard=large_memory  --user-syscall=$(top_srcdir)/syscall/build_x86_64/libodp_syscall.so --
endif