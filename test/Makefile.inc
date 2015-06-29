include $(top_srcdir)/Makefile.inc
include $(top_srcdir)/platform/@with_platform@/Makefile.inc
LIB   = $(top_builddir)/lib

#in the following line, the libs using the symbols should come before
#the libs containing them! The includer is given a chance to add things
#before libodp by setting PRE_LDADD before the inclusion.
LDADD = $(PRE_LDADD) $(LIB)/libodp.la

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

TESTS_ENVIRONMENT = ODP_PLATFORM=${with_platform} TEST_DIR="${builddir}" TARGET_RUNNER="$(KELF_LOG_COMPILER)" TOP_SRCDIR=$(top_srcdir)
TEST_EXTENSIONS = .kelf

if TARGET_IS_HW
TESTS_ENVIRONMENT += RUN_TARGET="k1-jtag"
endif
if TARGET_IS_SIMU
TESTS_ENVIRONMENT += RUN_TARGET="k1-cluster"
endif

KELF_LOG_COMPILER =	$(top_srcdir)/ktest-wrapper.sh
LOG_COMPILER =	$(top_srcdir)/ktest-wrapper.sh

