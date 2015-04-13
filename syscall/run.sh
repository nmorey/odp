#!/bin/sh -ex

cd $(dirname $(readlink -f ${0}))

mkdir -p build_x86_64/
cd build_x86_64/
cmake .. -Dk1_tools=$K1_TOOLCHAIN_DIR -DCMAKE_BUILD_TYPE=Debug 
make VERBOSE=1
cd ..

