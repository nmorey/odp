APP_DIR   := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
TEST_NAME := $(lastword $(subst /, ,$(APP_DIR)))

include $(APP_DIR)/../Makefile.apps

cflags += -I$(APP_DIR)

fast-ipsec-srcs := $(wildcard $(APP_DIR)/odp_ipsec*.[ch])
fast-ipsec-nameinmpk := odp_ipsec.kelf
cluster-bin += fast-ipsec

generator-srcs := $(wildcard $(APP_DIR)/odp_generator*.[ch])
generator-nameinmpk := odp_generator.kelf
cluster-bin += generator

$(realpath .)/firmware-ab01/firmware.kelf-nameinmpk := firmware-ab01.kelf
$(realpath .)/firmware-konic80/firmware.kelf-nameinmpk := firmware-konic80.kelf
$(TEST_NAME)-objs := fast-ipsec generator $(realpath .)/firmware-ab01/firmware.kelf $(realpath .)/firmware-konic80/firmware.kelf
mppa-bin += $(TEST_NAME)

$(TEST_NAME)-sim-cmd := $(IOPCIE_NAME) -c $(TEST_NAME) -c $(TEST_NAME) -c $(TEST_NAME) -c $(TEST_NAME)
$(TEST_NAME)-multibinary := $(TEST_NAME).mpk
mppa-tests += $(TEST_NAME)
$(TEST_NAME)-scripts := run.sh

pre-build-hooks := build-firmware
clean-hooks := clean-firmware

include $(TOPDIR)/../mk/Makefile.kalray

build-firmware:
	+mkdir -p ./firmware-ab01 && cd ./firmware-ab01 && \
	$(MAKE) -f $(APP_DIR)/firmware/Makefile CC="$(k1b-kalray-iounified_CC)" CFLAGS="$(k1b-kalray-iounified_CFLAGS)" \
	 LDFLAGS="$(k1b-kalray-iounified_LDFLAGS)" all
	+mkdir -p ./firmware-konic80 && cd ./firmware-konic80 && \
	$(MAKE) -f $(APP_DIR)/firmware/Makefile CC="$(k1b-kalray-iounified_konic80_CC)" CFLAGS="$(k1b-kalray-iounified_konic80_CFLAGS)" \
	 LDFLAGS="$(k1b-kalray-iounified_konic80_LDFLAGS)" all
clean-firmware:
	if [ -d ./firmware-ab01 ]; then \
	$(MAKE) -f $(APP_DIR)/firmware/Makefile CC="$(k1b-kalray-iounified_CC)" CFLAGS="$(k1b-kalray-iounified_CFLAGS)" \
	 LDFLAGS="$(k1b-kalray-iounified_LDFLAGS)" clean; else true; fi
	if [ -d ./firmware-konic80 ]; then \
	$(MAKE) -f $(APP_DIR)/firmware/Makefile CC="$(k1b-kalray-iounified_konic80_CC)" CFLAGS="$(k1b-kalray-iounified_konic80_CFLAGS)" \
	 LDFLAGS="$(k1b-kalray-iounified_konic80_LDFLAGS)" clean; else true; fi

