CONFIGS :=
_CONFIGS :=

$(foreach FIRM, $(FIRMWARES), $(eval $(FIRM)_CONFIGS := ))
$(foreach FIRM, $(FIRMWARES), $(eval _$(FIRM)_CONFIGS := ))

#
#
#
# K1B Definitions
#
#
#
k1b-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeos_CONF_OPTS   :=
k1b-kalray-nodeos_CFLAGS      := -march=k1b -DK1_NODEOS
k1b-kalray-nodeos_LDFLAGS     :=
k1b-kalray-nodeos_PLATFORM    := mppa
k1b-kalray-nodeos_INC_DIR     := k1b-nodeos
k1b-kalray-nodeos_MAKE_VALID  := -j1
k1b-kalray-nodeos_INSTALL     := true
CONFIGS += k1b-kalray-nodeos

k1b-kalray-nodeos_explorer_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeos_explorer_CONF_OPTS   :=
k1b-kalray-nodeos_explorer_CFLAGS      := -march=k1b -DK1_NODEOS -mboard=explorer -DK1B_EXPLORER
k1b-kalray-nodeos_explorer_LDFLAGS     :=
k1b-kalray-nodeos_explorer_PLATFORM    := mppa
k1b-kalray-nodeos_explorer_INC_DIR     := k1b-nodeos
k1b-kalray-nodeos_explorer_MAKE_VALID  := -j1
k1b-kalray-nodeos_explorer_INSTALL     := true
CONFIGS += k1b-kalray-nodeos_explorer

k1b-kalray-nodeos_simu_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeos_simu_CONF_OPTS   :=
k1b-kalray-nodeos_simu_CFLAGS      := -march=k1b -DK1_NODEOS -DMAGIC_SCALL -mboard=large_memory
k1b-kalray-nodeos_simu_LDFLAGS     :=
k1b-kalray-nodeos_simu_PLATFORM    := mppa
k1b-kalray-nodeos_simu_INC_DIR     := k1b-nodeos
k1b-kalray-nodeos_simu_MAKE_VALID  :=
k1b-kalray-nodeos_simu_INSTALL     := true
_CONFIGS += k1b-kalray-nodeos_simu

k1b-kalray-rtems_CONF_ENV    := CC=k1-gcc  CXX=k1-rtems-g++ STRIP=k1-strip
k1b-kalray-rtems_CONF_OPTS   :=
k1b-kalray-rtems_CFLAGS      := -march=k1b -DK1_RTEMS -mboard=explorer -mos=rtems -mcore=k1io -mcluster=ioddr -mprocessing=single
k1b-kalray-rtems_LDFLAGS     :=
k1b-kalray-rtems_PLATFORM    := mppa
k1b-kalray-rtems_INC_DIR     := k1b-rtems
k1b-kalray-rtems_MAKE_VALID  := -j1
k1b-kalray-rtems_INSTALL     := true
_CONFIGS += k1b-kalray-rtems

k1b-kalray-mos_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mos_CONF_OPTS   :=
k1b-kalray-mos_CFLAGS      := -march=k1b -mhypervisor -DK1_MOS
k1b-kalray-mos_LDFLAGS     := -mhypervisor -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1b-kalray-mos_PLATFORM    := mppa
k1b-kalray-mos_INC_DIR     := k1b-elf
k1b-kalray-mos_MAKE_VALID  := -j1
k1b-kalray-mos_INSTALL     := true
k1b-kalray-mos_INSTALL_DOC := true
CONFIGS += k1b-kalray-mos

k1b-kalray-mos_explorer_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mos_explorer_CONF_OPTS   :=
k1b-kalray-mos_explorer_CFLAGS      := -march=k1b -mhypervisor -DK1_MOS -mboard=explorer -DK1B_EXPLORER
k1b-kalray-mos_explorer_LDFLAGS     := -mhypervisor -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1b-kalray-mos_explorer_PLATFORM    := mppa
k1b-kalray-mos_explorer_INC_DIR     := k1b-elf
k1b-kalray-mos_explorer_MAKE_VALID  := -j1
k1b-kalray-mos_explorer_INSTALL     := true
CONFIGS += k1b-kalray-mos_explorer

k1b-kalray-mos_simu_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mos_simu_CONF_OPTS   :=
k1b-kalray-mos_simu_CFLAGS      := -march=k1b -mhypervisor -DK1_MOS -DMAGIC_SCALL -mboard=large_memory
k1b-kalray-mos_simu_LDFLAGS     := -mhypervisor -lvbsp -lutask -Wl,--defsym=USER_STACK_SIZE=4096
k1b-kalray-mos_simu_PLATFORM    := mppa
k1b-kalray-mos_simu_INC_DIR     := k1b-elf
k1b-kalray-mos_simu_MAKE_VALID  :=
k1b-kalray-mos_simu_INSTALL     := true
_CONFIGS += k1b-kalray-mos_simu

#
#
#
# x86 Definitions
#
#
#
x86_64-unknown-linux-gnu_CONF_ENV    :=
x86_64-unknown-linux-gnu_CONF_OPTS   :=
x86_64-unknown-linux-gnu_CFLAGS      :=
x86_64-unknown-linux-gnu_LDFLAGS     :=
x86_64-unknown-linux-gnu_PLATFORM    := linux-generic
x86_64-unknown-linux-gnu_INC_DIR     :=
x86_64-unknown-linux-gnu_MAKE_VALID  :=
x86_64-unknown-linux-gnu_INSTALL     := false
CONFIGS += x86_64-unknown-linux-gnu

#
#
#
# Firmware Definitions
#
#
#

k1b-kalray-ioddr_CC          := k1-gcc
k1b-kalray-ioddr_CFLAGS      := -march=k1b -mos=bare -mcore=k1bio -mcluster=ioddr -mprocessing=single
k1b-kalray-ioddr_LDFLAGS     :=
k1b-kalray-ioddr_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b
iopcie_CONFIGS += k1b-kalray-ioddr

k1b-kalray-ioddr_explorer_CC          := k1-gcc
k1b-kalray-ioddr_explorer_CFLAGS      := -march=k1b -mos=bare -mcore=k1bio -mcluster=ioddr \
									  	 -mprocessing=single -mboard=explorer -DK1B_EXPLORER
k1b-kalray-ioddr_explorer_LDFLAGS     :=
k1b-kalray-ioddr_explorer_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b_explorer
iopcie_CONFIGS += k1b-kalray-ioddr_explorer

k1b-kalray-ioeth530_CC          := k1-gcc
k1b-kalray-ioeth530_CFLAGS      := -march=k1b -DK1_BARE -mcore=k1bio -mcluster=ioeth -mos=bare -mboard=eth_530 -DK1B_EXPLORER
k1b-kalray-ioeth530_LDFLAGS     :=
k1b-kalray-ioeth530_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b_explorer
ioeth_CONFIGS += k1b-kalray-ioeth530

k1b-kalray-iounified_CC          := k1-gcc
k1b-kalray-iounified_CFLAGS      := -march=k1b -DK1_BARE -mcore=k1bio -mcluster=ioddr -mos=bare
k1b-kalray-iounified_LDFLAGS     :=
k1b-kalray-iounified_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b
iounified_CONFIGS += k1b-kalray-iounified

k1b-kalray-iounified-nopcie_CC          := k1-gcc
k1b-kalray-iounified-nopcie_CFLAGS      := -march=k1b -DK1_BARE -mcore=k1bio -mcluster=ioddr -mos=bare
k1b-kalray-iounified-nopcie_LDFLAGS     :=
k1b-kalray-iounified-nopcie_INSTDIR      = $(K1ST_DIR)/share/odp/firmware/k1b
_iounified-nopcie_CONFIGS += k1b-kalray-iounified-nopcie
