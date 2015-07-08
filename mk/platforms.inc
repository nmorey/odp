CONFIGS :=
_CONFIGS :=

boot_CONFIGS  :=
ioeth_CONFIGS :=

k1a-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1a-kalray-nodeos_CONF_OPTS   :=
k1a-kalray-nodeos_CFLAGS      := -march=k1a -DK1_NODEOS
k1a-kalray-nodeos_LDFLAGS     :=
k1a-kalray-nodeos_PLATFORM    := k1-cluster
k1a-kalray-nodeos_INC_DIR     := k1a-nodeos
k1a-kalray-nodeos_MAKE_VALID  := -j1
k1a-kalray-nodeos_INSTALL     := true
k1a-kalray-nodeos_INSTALL_DOC := true
CONFIGS += k1a-kalray-nodeos

k1a-kalray-nodeosmagic_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1a-kalray-nodeosmagic_CONF_OPTS   :=
k1a-kalray-nodeosmagic_CFLAGS      := -march=k1a -DK1_NODEOS -DMAGIC_SCALL -mboard=large_memory
k1a-kalray-nodeosmagic_LDFLAGS     :=
k1a-kalray-nodeosmagic_PLATFORM    := k1-cluster
k1a-kalray-nodeosmagic_INC_DIR     := k1a-nodeos
k1a-kalray-nodeosmagic_MAKE_VALID  :=
k1a-kalray-nodeosmagic_INSTALL     := true
CONFIGS += k1a-kalray-nodeosmagic

k1a-kalray-rtems_CONF_ENV    := CC=k1-gcc  CXX=k1-rtems-g++ STRIP=k1-strip
k1a-kalray-rtems_CONF_OPTS   :=
k1a-kalray-rtems_CFLAGS      := -march=k1a -DK1_RTEMS -mos=rtems -mcore=k1io -mcluster=ioddr -mprocessing=single
k1a-kalray-rtems_LDFLAGS     :=
k1a-kalray-rtems_PLATFORM    := k1-cluster
k1a-kalray-rtems_INC_DIR     := k1a-rtems
k1a-kalray-rtems_MAKE_VALID  := -j1
k1a-kalray-rtems_INSTALL     := true
k1a-kalray-rtems_INSTALL_DOC := true
_CONFIGS += k1a-kalray-rtems

k1b-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeos_CONF_OPTS   :=
k1b-kalray-nodeos_CFLAGS      := -march=k1b -DK1_NODEOS -mboard=explorer
k1b-kalray-nodeos_LDFLAGS     :=
k1b-kalray-nodeos_PLATFORM    := k1-cluster
k1b-kalray-nodeos_INC_DIR     := k1b-nodeos
k1b-kalray-nodeos_MAKE_VALID  := -j1
k1b-kalray-nodeos_INSTALL     := true
CONFIGS += k1b-kalray-nodeos

k1b-kalray-nodeosmagic_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeosmagic_CONF_OPTS   :=
k1b-kalray-nodeosmagic_CFLAGS      := -march=k1b -DK1_NODEOS -DMAGIC_SCALL -mboard=large_memory
k1b-kalray-nodeosmagic_LDFLAGS     :=
k1b-kalray-nodeosmagic_PLATFORM    := k1-cluster
k1b-kalray-nodeosmagic_INC_DIR     := k1b-nodeos
k1b-kalray-nodeosmagic_MAKE_VALID  :=
k1b-kalray-nodeosmagic_INSTALL     := true
_CONFIGS += k1b-kalray-nodeosmagic

k1b-kalray-rtems_CONF_ENV    := CC=k1-gcc  CXX=k1-rtems-g++ STRIP=k1-strip
k1b-kalray-rtems_CONF_OPTS   :=
k1b-kalray-rtems_CFLAGS      := -march=k1b -DK1_RTEMS -mboard=explorer -mos=rtems -mcore=k1io -mcluster=ioddr -mprocessing=single
k1b-kalray-rtems_LDFLAGS     :=
k1b-kalray-rtems_PLATFORM    := k1-cluster
k1b-kalray-rtems_INC_DIR     := k1b-rtems
k1b-kalray-rtems_MAKE_VALID  := -j1
k1b-kalray-rtems_INSTALL     := true
_CONFIGS += k1b-kalray-rtems

k1a-kalray-mos_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1a-kalray-mos_CONF_OPTS   :=
k1a-kalray-mos_CFLAGS      := -march=k1a -mhypervisor -DK1_MOS
k1a-kalray-mos_LDFLAGS     := -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1a-kalray-mos_PLATFORM    := k1-cluster
k1a-kalray-mos_INC_DIR     := k1a-elf
k1a-kalray-mos_MAKE_VALID  := -j1
k1a-kalray-mos_INSTALL     := true
CONFIGS += k1a-kalray-mos

k1a-kalray-mosmagic_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1a-kalray-mosmagic_CONF_OPTS   :=
k1a-kalray-mosmagic_CFLAGS      := -march=k1a -mhypervisor -DK1_MOS -DMAGIC_SCALL -mboard=large_memory
k1a-kalray-mosmagic_LDFLAGS     := -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1a-kalray-mosmagic_PLATFORM    := k1-cluster
k1a-kalray-mosmagic_INC_DIR     := k1a-elf
k1a-kalray-mosmagic_MAKE_VALID  :=
k1a-kalray-mosmagic_INSTALL     := true
CONFIGS += k1a-kalray-mosmagic

k1b-kalray-mos_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mos_CONF_OPTS   :=
k1b-kalray-mos_CFLAGS      := -march=k1b -mhypervisor -DK1_MOS -mboard=explorer
k1b-kalray-mos_LDFLAGS     := -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1b-kalray-mos_PLATFORM    := k1-cluster
k1b-kalray-mos_INC_DIR     := k1b-elf
k1b-kalray-mos_MAKE_VALID  := -j1
k1b-kalray-mos_INSTALL     := true
CONFIGS += k1b-kalray-mos

k1b-kalray-mosmagic_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mosmagic_CONF_OPTS   :=
k1b-kalray-mosmagic_CFLAGS      := -march=k1b -mhypervisor -DK1_MOS -DMAGIC_SCALL -mboard=large_memory
k1b-kalray-mosmagic_LDFLAGS     := -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1b-kalray-mosmagic_PLATFORM    := k1-cluster
k1b-kalray-mosmagic_INC_DIR     := k1b-elf
k1b-kalray-mosmagic_MAKE_VALID  :=
k1b-kalray-mosmagic_INSTALL     := true
_CONFIGS += k1b-kalray-mosmagic

x86_64-unknown-linux-gnu_CONF_ENV    :=
x86_64-unknown-linux-gnu_CONF_OPTS   :=
x86_64-unknown-linux-gnu_CFLAGS      :=
x86_64-unknown-linux-gnu_LDFLAGS     :=
x86_64-unknown-linux-gnu_PLATFORM    := linux-generic
x86_64-unknown-linux-gnu_INC_DIR     :=
x86_64-unknown-linux-gnu_MAKE_VALID  :=
x86_64-unknown-linux-gnu_INSTALL     := false
CONFIGS += x86_64-unknown-linux-gnu

k1a-kalray-ioddr_CC          := k1-gcc
k1a-kalray-ioddr_CFLAGS      := -march=k1a -DK1_RTEMS -mos=rtems -mcore=k1io -mcluster=ioddr -mprocessing=single
k1a-kalray-ioddr_LDFLAGS     := -lmppapower -lmppanoc -lmpparouting
k1a-kalray-ioddr_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1a
boot_CONFIGS += k1a-kalray-ioddr

k1b-kalray-ioddr_CC          := k1-gcc
k1b-kalray-ioddr_CFLAGS      := -march=k1b -DK1_RTEMS -mos=rtems -mcore=k1bio -mcluster=ioddr -mprocessing=single
k1b-kalray-ioddr_LDFLAGS     := -lmppapower -lmppanoc -lmpparouting
k1b-kalray-ioddr_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b
boot_CONFIGS += k1b-kalray-ioddr

# k1a-kalray-ioeth_CC          := k1-gcc
# k1a-kalray-ioeth_CFLAGS      := -march=k1a -DK1_BARE -mcore=k1io -mcluster=ioeth
# k1a-kalray-ioeth_LDFLAGS     := -lmppaeth
# k1a-kalray-ioeth_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1a
# ioeth_CONFIGS += k1a-kalray-ioeth

k1b-kalray-ioeth_CC          := k1-gcc
k1b-kalray-ioeth_CFLAGS      := -march=k1b -DK1_BARE -mcore=k1bio -mcluster=ioeth -mos=bare
k1b-kalray-ioeth_LDFLAGS     := -lmppaeth -lmppapower -lmppanoc -lmpparouting
k1b-kalray-ioeth_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b
ioeth_CONFIGS += k1b-kalray-ioeth

k1b-kalray-ioeth530_CC          := k1-gcc
k1b-kalray-ioeth530_CFLAGS      := -march=k1b -DK1_BARE -mcore=k1bio -mcluster=ioeth -mos=bare -mboard=eth_530
k1b-kalray-ioeth530_LDFLAGS     := -lmppaeth -lmppapower -lmppanoc -lmpparouting
k1b-kalray-ioeth530_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b
ioeth_CONFIGS += k1b-kalray-ioeth530
