O ?= ./output

project-name := odp

ODP_SRC_DIR ?= $(CURDIR)/..
ODP_PLATFORM ?= mppa
ODP_ARCH ?= k1a-kalray-nodeos
# Kalray internal system name
SYSTEM_NAME = $(subst -kalray,,$(ODP_ARCH))

ifeq ($(MAKELEVEL),0)
$(info --- Building for arch $(ODP_ARCH), platform $(ODP_PLATFORM) ---)
endif

CUNIT_COMMON_SRC := $(ODP_SRC_DIR)/test/validation/common/odp_cunit_common.c

ifeq ($(USE_PACKAGES),1)

cluster-cflags := -I$(K1_TOOLCHAIN_DIR)/$(SYSTEM_NAME)/include -I$(K1_TOOLCHAIN_DIR)/kalray_internal/cunit/$(ODP_ARCH)/include
cluster-lflags := -L$(K1_TOOLCHAIN_DIR)/lib/$(ODP_ARCH)/ -L$(K1_TOOLCHAIN_DIR)/kalray_internal/cunit/$(ODP_ARCH)/lib/

else

# Use the local install
ODP_INSTALL_DIR=$(ODP_SRC_DIR)/install/local/k1tools/
CUNIT_INSTALL_DIR=$(ODP_SRC_DIR)/cunit/install/local/k1tools/kalray_internal/cunit/$(ODP_ARCH)/

cluster-cflags := -I$(ODP_INSTALL_DIR)/$(SYSTEM_NAME)/include -I$(CUNIT_INSTALL_DIR)/include
cluster-lflags := -L$(ODP_INSTALL_DIR)/lib/$(ODP_ARCH)/ -L$(CUNIT_INSTALL_DIR)/lib/

endif

cluster-cflags += -I$(ODP_SRC_DIR)/test/validation/common/
cluster-lflags += -lodphelper -lodp -lcunit -lmppanoc -lmpparouting

# Empty direct assignement to allows += to work in included Makefiles
cluster-bin :=
cluster-tests :=

spawner-srcs := spawner.c
spawner-lflags := -lmppapower -lmppanoc -lmpparouting

io-bin := spawner

# Subdirectories
SUB_DIRS := basic_cluster
SUB_DIRS := $(addsuffix /Makefile,$(SUB_DIRS))

include $(SUB_DIRS)

# Inject cunit common file to all tests
$(foreach clus_bin,$(cluster-bin),$(eval $(clus_bin)-srcs += $(CUNIT_COMMON_SRC)))

include $(CURDIR)/Makefile.kalray
