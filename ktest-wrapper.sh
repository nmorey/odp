#!/bin/bash
ELF=$1
shift
extension="${ELF##*.}"
if [ "$extension" != "kelf" ]; then
	exec ${ELF} $*
fi
case "$RUN_TARGET" in
	"k1-jtag")
		exec k1-jtag-runner --exec-file "Cluster0:$ELF" -- $*
		;;
	"k1-cluster")
		exec k1-cluster   --functional --dcache-no-check  --mboard=large_memory  --user-syscall=${TOP_SRCDIR}/syscall/build_x86_64/libodp_syscall.so -- $*
		;;
	*)
		exec ${ELF} $*
esac
