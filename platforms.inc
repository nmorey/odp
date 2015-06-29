CONFIGS :=

k1a-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1a-kalray-nodeos_CONF_OPTS   :=
k1a-kalray-nodeos_PLATFORM    := k1-cluster
k1a-kalray-nodeos_INC_DIR     := k1a-nodeos
k1a-kalray-nodeos_MAKE_VALID  := -j1
k1a-kalray-nodeos_BUILD_TESTS := true
k1a-kalray-nodeos_INSTALL     := true
k1a-kalray-nodeos_INSTALL_DOC := true
CONFIGS += k1a-kalray-nodeos

k1a-kalray-nodeosmagic_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1a-kalray-nodeosmagic_CONF_OPTS   :=
k1a-kalray-nodeosmagic_PLATFORM    := k1-cluster
k1a-kalray-nodeosmagic_INC_DIR     := k1a-nodeos
k1a-kalray-nodeosmagic_MAKE_VALID  :=
k1a-kalray-nodeosmagic_BUILD_TESTS := true
k1a-kalray-nodeosmagic_INSTALL     := true
CONFIGS += k1a-kalray-nodeosmagic

k1b-kalray-nodeos_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeos_CONF_OPTS   :=
k1b-kalray-nodeos_PLATFORM    := k1-cluster
k1b-kalray-nodeos_INC_DIR     := k1b-nodeos
k1b-kalray-nodeos_MAKE_VALID  := -j1
k1b-kalray-nodeos_BUILD_TESTS := true
k1b-kalray-nodeos_INSTALL     := true
CONFIGS += k1b-kalray-nodeos

k1b-kalray-nodeosmagic_CONF_ENV    := CC=k1-nodeos-gcc  CXX=k1-nodeos-g++ STRIP=k1-strip
k1b-kalray-nodeosmagic_CONF_OPTS   :=
k1b-kalray-nodeosmagic_PLATFORM    := k1-cluster
k1b-kalray-nodeosmagic_INC_DIR     := k1b-nodeos
k1b-kalray-nodeosmagic_MAKE_VALID  :=
k1b-kalray-nodeosmagic_BUILD_TESTS := true
k1b-kalray-nodeosmagic_INSTALL     := true
_CONFIGS += k1b-kalray-nodeosmagic


k1a-kalray-mos_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1a-kalray-mos_CONF_OPTS   :=
k1a-kalray-mos_PLATFORM    := k1-cluster
k1a-kalray-mos_INC_DIR    := k1a-elf
k1a-kalray-mos_MAKE_VALID  := -j1
k1a-kalray-mos_BUILD_TESTS := true
k1a-kalray-mos_INSTALL     := true
_CONFIGS += k1a-kalray-mos

k1a-kalray-mosmagic_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1a-kalray-mosmagic_CONF_OPTS   :=
k1a-kalray-mosmagic_PLATFORM    := k1-cluster
k1a-kalray-mosmagic_INC_DIR    := k1a-elf
k1a-kalray-mosmagic_MAKE_VALID  :=
k1a-kalray-mosmagic_BUILD_TESTS := true
k1a-kalray-mosmagic_INSTALL     := true
_CONFIGS += k1a-kalray-mosmagic

k1b-kalray-mos_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mos_CONF_OPTS   :=
k1b-kalray-mos_PLATFORM    := k1-cluster
k1b-kalray-mos_INC_DIR    := k1b-elf
k1b-kalray-mos_MAKE_VALID  := -j1
k1b-kalray-mos_BUILD_TESTS := true
k1b-kalray-mos_INSTALL     := true
_CONFIGS += k1b-kalray-mos

k1b-kalray-mosmagic_CONF_ENV    := CC=k1-gcc  CXX=k1-g++ STRIP=k1-strip
k1b-kalray-mosmagic_CONF_OPTS   :=
k1b-kalray-mosmagic_PLATFORM    := k1-cluster
k1b-kalray-mosmagic_INC_DIR    := k1b-elf
k1b-kalray-mosmagic_MAKE_VALID  :=
k1b-kalray-mosmagic_BUILD_TESTS := true
k1b-kalray-mosmagic_INSTALL     := true
_CONFIGS += k1b-kalray-mosmagic

x86_64-unknown-linux-gnu_CONF_ENV    :=
x86_64-unknown-linux-gnu_CONF_OPTS   :=
x86_64-unknown-linux-gnu_PLATFORM    := linux-generic
x86_64-unknown-linux-gnu_INC_DIR     := 
x86_64-unknown-linux-gnu_MAKE_VALID  :=
x86_64-unknown-linux-gnu_BUILD_TESTS := true
x86_64-unknown-linux-gnu_INSTALL     := false
CONFIGS += x86_64-unknown-linux-gnu
