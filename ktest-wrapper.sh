#!/bin/bash
ELF=$1
shift
extension="${ELF##*.}"
if [ "$extension" != "kelf" ]; then
	exec ${ELF} $*
fi

case "$RUN_TARGET" in
	"k1-jtag")
		BOARD_TYPE=$(cat /mppa/board0/type)
		FIRMWARES=""
		case "$BOARD_TYPE" in
			"ab01")
				FIRMWARES="--exec-file IODDR0:${TOP_BUILDDIR}/../iounified/k1b-kalray-iounified/iounified.kelf --exec-file IODDR1:${TOP_BUILDDIR}/../iounified/k1b-kalray-iounified/iounified.kelf "
				;;
			"explorer")
				FIRMWARES="--exec-file IOETH1:${TOP_BUILDDIR}/../ioeth/k1b-kalray-ioeth530/ioeth.kelf"
				;;
			"")
				;;
			*)
				;;
		esac

		echo k1-jtag-runner ${FIRMWARES} --exec-file "Cluster0:$ELF" -- $*
		exec k1-jtag-runner ${FIRMWARES} --exec-file "Cluster0:$ELF" -- $*
		;;
	"k1-cluster")
		echo k1-cluster   --functional --dcache-no-check  --mboard=large_memory  --user-syscall=${TOP_SRCDIR}/syscall/build_x86_64/libodp_syscall.so -- $ELF $*
		exec k1-cluster   --functional --dcache-no-check  --mboard=large_memory  --user-syscall=${TOP_SRCDIR}/syscall/build_x86_64/libodp_syscall.so -- $ELF $*
		;;
	*)
		echo ${ELF} $*
		exec ${ELF} $*
esac

