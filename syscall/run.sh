#!/bin/sh -ex

INSTALL_DIR=$1

if [ -z $INSTALL_DIR ]; then
	echo "Usage $0 <INSTALL_DIR>"
	exit 1
fi
INSTALL_DIR=$(readlink -m $INSTALL_DIR)

cd $(dirname $(readlink -f ${0}))
mkdir -p build_x86_64/
cd build_x86_64/
cmake .. -Dk1_tools=$K1_TOOLCHAIN_DIR -DCMAKE_BUILD_TYPE=Debug 
make VERBOSE=1
mkdir -p $INSTALL_DIR/lib64
install -- libodp_syscall.so $INSTALL_DIR/lib64
cd ..

