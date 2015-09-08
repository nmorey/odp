O ?= ./output

project-name := odp

ODP_SRC_DIR ?= $(CURDIR)/..
ODP_ARCH ?= k1b-kalray-nodeos

include $(ODP_SRC_DIR)/mk/platforms.inc

INCLUDE_DIR=$($(ODP_ARCH)_INC_DIR)

# Kalray internal system name
SYSTEM_NAME = $(subst -kalray,,$(ODP_ARCH))
CORE_ARCH = $(firstword $(subst -, ,$(ODP_ARCH)))
OS_BOARD_NAME = $(lastword $(subst -, ,$(ODP_ARCH)))

# -mos option according to real os name
OS_mos := bare
OS_nodeos := nodeos
OS_rtems := rtems
BOARD_simu := developer
BOARD_explorer := explorer

# extract OS name and board name
REAL_OS_NAME = $(firstword $(subst _, ,$(OS_BOARD_NAME)))
OS_NAME = $(OS_$(REAL_OS_NAME))
BOARD_NAME = $(if $(findstring _,$(OS_BOARD_NAME)),$(BOARD_$(lastword $(subst _, ,$(OS_BOARD_NAME)))),developer)

cluster-system := $(OS_NAME)
arch = $(CORE_ARCH)
board = $(BOARD_NAME)

cflags := -Wall -Werror -Wextra

ifeq ($(MAKELEVEL),0)
$(info --- Building for arch $(arch), os $(OS_NAME), board $(BOARD_NAME) ---)
endif

CUNIT_COMMON_SRC := $(ODP_SRC_DIR)/test/validation/common/odp_cunit_common.c
LIB_NOC_LFLAGS :=  -lmppapower -lmpparouting -lmppanoc
ifeq ($(USE_PACKAGES),1)
  ODP_INSTALL_DIR = $(K1_TOOLCHAIN_DIR)
  CUNIT_INSTALL_DIR = $(K1_TOOLCHAIN_DIR)/kalray_internal/cunit/$(ODP_ARCH)
  IOPCIE_ELF := $(ODP_INSTALL_DIR)/share/odp/firmware/$(CORE_ARCH)$(if $(findstring explorer,$(BOARD_NAME)),_explorer,)/iopcie.kelf
else
  ODP_INSTALL_DIR = $(ODP_SRC_DIR)/install/local/k1tools/
  CUNIT_INSTALL_DIR = $(ODP_SRC_DIR)/cunit/install/local/k1tools/kalray_internal/cunit/$(ODP_ARCH)/

  # Build the ioddr boot firmware to avoid additionnal build/install steps...
  iopcie-srcs := $(ODP_SRC_DIR)/firmware/iopcie/ioddr-boot.c $(ODP_SRC_DIR)/firmware/common/rpc/rpc-server.c
  iopcie-srcs += $(ODP_SRC_DIR)/platform/mppa/odp_rpc.c $(wildcard $(ODP_SRC_DIR)/firmware/common/pcie/*.c)
  iopcie-system := bare
  iopcie-lflags := $(LIB_NOC_LFLAGS)
  iopcie-cflags := -I$(ODP_SRC_DIR)/platform/mppa/include/ -I$(ODP_SRC_DIR)/firmware/rpc/
  iopcie-cflags += -I$(ODP_SRC_DIR)/firmware/common/rpc/ -I$(ODP_SRC_DIR)/firmware/common/pcie/ -I$(ODP_SRC_DIR)/include/
  IOPCIE_ELF := iopcie
endif

cluster-cflags := -I$(ODP_INSTALL_DIR)/$(INCLUDE_DIR)/include -I$(CUNIT_INSTALL_DIR)/include
cluster-lflags := -L$(ODP_INSTALL_DIR)/lib/$(ODP_ARCH)/ -L$(CUNIT_INSTALL_DIR)/lib/ 

cluster-cflags += -I$(ODP_SRC_DIR)/test/validation/common/ -I$(CURDIR)/include $($(ODP_ARCH)_CFLAGS)
cluster-lflags += -lodphelper -lodp -lcunit $(LIB_NOC_LFLAGS) $($(ODP_ARCH)_LDFLAGS)

$(IOPCIE_ELF)-nameinmpk = iopcie
IOPCIE_NAME := iopcie

# Empty direct assignement to allows += to work in included Makefiles
io-bin := $(IOPCIE_ELF)
mppa-bin :=
cluster-bin :=
cluster-tests :=
mppa-tests :=

# Subdirectories
SUB_DIRS := $(wildcard */Makefile) 
include $(SUB_DIRS)

include $(CURDIR)/../mk/Makefile.kalray
