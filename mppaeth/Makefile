obj-m += mppapcie_netdev.o

TOOLCHAIN_DIR ?= $(K1_TOOLCHAIN_DIR)
MPPAPCIEDIR ?= $(shell ls -d $$(readlink -f $(TOOLCHAIN_DIR))/../../src/k1-mppapcie-dkms-*)
KERNELBUILD_DIR ?= /lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS += -I${TOOLCHAIN_DIR}/include -I$(PWD) -Wall -Werror -I$(MPPAPCIEDIR)

KBUILD_EXTRA_SYMBOLS += $(MPPAPCIEDIR)/Module.symvers

DEBUG ?= 0
ifeq ($(DEBUG), 1)
	GCOV_PROFILE := y
	EXTRA_CFLAGS += -DDEBUG
endif

ifeq ($(CONFIG_MPPA_TRACE), 1)
	EXTRA_CFLAGS += -fno-inline
endif

MULTI_IO ?= 0
ifeq ($(MULTI_IO), 1)
	EXTRA_CFLAGS += -DMULTI_IO
endif

mppapcie_netdev-objs += mppa_pcie_netdev.o

all:
	@echo "---------------------------------------------------------------------"
ifeq ($(DEBUG), 1)
	@echo "		   DEBUG is enabled, GCOV support enabled"
else
	@echo "		DEBUG is not enabled (make DEBUG=1 to enable)"
endif
	@echo "---------------------------------------------------------------------"
	make -C $(KERNELBUILD_DIR) M=$(PWD) modules

clean:
	make -C $(KERNELBUILD_DIR) M=$(PWD) clean
