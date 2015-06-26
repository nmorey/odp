O ?= ./output

project-name := odp

ODP_SRC_DIR ?= $(CURDIR)/../
ODP_PLATFORM ?= k1-cluster
ODP_ARCH ?= k1a-kalray-nodeos

ODP_INSTALL_DIR=$(ODP_SRC_DIR)/install/local/k1tools/
CUNIT_INSTALL_DIR=$(ODP_SRC_DIR)/cunit/install/$(ODP_ARCH)/
# Kalray internal system name
SYSTEM_NAME = $(subst -kalray,,$(ODP_ARCH))

CUNIT_COMMON_SRC := $(ODP_SRC_DIR)/test/validation/common/odp_cunit_common.c

# Assign all flasg for test build
cluster-cflags := -I$(ODP_INSTALL_DIR)/$(SYSTEM_NAME)/include  -I$(ODP_SRC_DIR)/test/validation/common/
cluster-cflags += -I$(CUNIT_INSTALL_DIR)/include

cluster-lflags := -L$(ODP_INSTALL_DIR)/lib/$(ODP_ARCH)/ -lodp
cluster-lflags += -L$(CUNIT_INSTALL_DIR)/lib/ -lcunit

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

include $(K1_TOOLCHAIN_DIR)/share/make/Makefile.kalray
