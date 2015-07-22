O ?= ./output

project-name := odp

ODP_SRC_DIR ?= $(CURDIR)/..
ODP_ARCH ?= k1a-kalray-nodeos

# Kalray internal system name
SYSTEM_NAME = $(subst -kalray,,$(ODP_ARCH))
CORE_ARCH = $(firstword $(subst -, ,$(ODP_ARCH)))
OS_BOARD_NAME = $(lastword $(subst -, ,$(ODP_ARCH)))

OS_mos := bare
OS_nodeos := nodeos
OS_rtems := rtems
BOARD_simu := developer
BOARD_explorer := explorer

OS_NAME = $(OS_$(firstword $(subst _, ,$(OS_BOARD_NAME))))
BOARD_NAME = $(if $(findstring _,$(OS_BOARD_NAME)),$(BOARD_$(lastword $(subst _, ,$(OS_BOARD_NAME)))),developer)

cluster-system := $(OS_NAME)
arch = $(CORE_ARCH)
board = $(BOARD_NAME)

cflags := -Wall -Werror

ifeq ($(MAKELEVEL),0)
$(info --- Building for arch $(arch), os $(OS_NAME), board $(BOARD_NAME) ---)
endif

CUNIT_COMMON_SRC := $(ODP_SRC_DIR)/test/validation/common/odp_cunit_common.c
LIB_NOC_LFLAGS := -lmppapower -lmppanoc -lmpparouting
ifeq ($(USE_PACKAGES),1)
  ODP_INSTALL_DIR = $(K1_TOOLCHAIN_DIR)
  CUNIT_INSTALL_DIR = $(K1_TOOLCHAIN_DIR)/kalray_internal/cunit/$(ODP_ARCH)
  BOOT_ELF := $(ODP_INSTALL_DIR)/share/odp/firmware/$(CORE_ARCH)/boot.kelf
else
  ODP_INSTALL_DIR = $(ODP_SRC_DIR)/install/local/k1tools/
  CUNIT_INSTALL_DIR = $(ODP_SRC_DIR)/cunit/install/local/k1tools/kalray_internal/cunit/$(ODP_ARCH)/

  # Build the ioddr boot firmware to avoid additionnal build/install steps...
  boot-srcs := $(ODP_SRC_DIR)/firmware/boot/ioddr-boot.c $(ODP_SRC_DIR)/firmware/rpc/rpc-server.c $(ODP_SRC_DIR)/platform/mppa/odp_rpc.c
  boot-lflags := $(LIB_NOC_LFLAGS)
  boot-cflags := -I$(ODP_SRC_DIR)/platform/mppa/include/ -I$(ODP_SRC_DIR)/firmware/rpc/ -I$(ODP_SRC_DIR)/include/
  BOOT_ELF := boot
endif

cluster-cflags := -I$(ODP_INSTALL_DIR)/$(arch)-$(cluster-system)/include -I$(CUNIT_INSTALL_DIR)/include
cluster-lflags := -L$(ODP_INSTALL_DIR)/lib/$(ODP_ARCH)/ -L$(CUNIT_INSTALL_DIR)/lib/ $(LIB_NOC_LFLAGS)

cluster-cflags += -I$(ODP_SRC_DIR)/test/validation/common/
cluster-lflags += -lodphelper -lodp -lcunit -lmppanoc -lmpparouting

$(BOOT_ELF)-nameinmpk = boot

# Empty direct assignement to allows += to work in included Makefiles
io-bin := $(BOOT_ELF)
mppa-bin :=
cluster-bin :=
cluster-tests :=
mppa-tests :=

# Subdirectories
SUB_DIRS := $(wildcard */Makefile) 
include $(SUB_DIRS)

include $(CURDIR)/../mk/Makefile.kalray
