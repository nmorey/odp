TEST_EXTENSIONS = .kelf

KELF_LOG_COMPILER =	$(top_srcdir)/ktest-wrapper.sh
LOG_COMPILER =	$(top_srcdir)/ktest-wrapper.sh

TESTS_ENVIRONMENT +=  TARGET_RUNNER="$(KELF_LOG_COMPILER)" TOP_SRCDIR=$(top_srcdir) TOP_BUILDDIR=$(top_builddir) RUN_TARGET=${RUN_TARGET} 