#!/bin/bash
ELF=$1
shift
k1-jtag-runner --exec-file "Cluster0:$ELF" -- $*
