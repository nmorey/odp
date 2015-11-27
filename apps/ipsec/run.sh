CUR_DIR=$(readlink -e $(dirname $0))
cd $CUR_DIR
exec k1-jtag-runner --progress --multibinary=ipsec.mpk --exec-multibin=IODDR0:firmware.kelf --exec-multibin=IODDR1:firmware.kelf --
