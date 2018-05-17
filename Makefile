NAME ?= charon
SUFFIX ?= .c
DIRS ?= .

ARCH=arm
VIVADO_SETTINGS ?= /opt/Xilinx/Vivado/2016.4/settings64.sh
HAVE_VIVADO= $(shell bash -c "source $(VIVADO_SETTINGS) > /dev/null 2>&1 && vivado -version > /dev/null 2>&1 && echo 1 || echo 0")

FLAGS ?= -O3 -std=c99 -I../buildroot/output/host/arm-buildroot-linux-gnueabi/sysroot/usr/include/\
         --sysroot=../buildroot/output/host/arm-buildroot-linux-gnueabi/sysroot/\
        -I./third_party/libtuntap/

SYSROOT ?= ../buildroot/output/host/arm-buildroot-linux-gnueabi/sysroot/

LDFLAGS ?= --sysroot=/opt/Xilinx/SDK/2016.4/gnu/arm/lin/arm-xilinx-linux-gnueabi/libc\
           -L ../buildroot/output/host/arm-buildroot-linux-gnueabi/sysroot/\
           -L /opt/Xilinx/SDK/2016.4/gnu/arm/lin/arm-xilinx-linux-gnueabi/libc\
           -L /opt/Xilinx/SDK/2016.4/gnu/arm/lin/arm-xilinx-linux-gnueabi/libc/lib\
           -L /opt/Xilinx/SDK/2016.4/gnu/arm/lin/arm-xilinx-linux-gnueabi/libc/usr/lib \
           -L ./third_party/libfec\
           -L ./third_party/libtuntap\
           -L$(SYSROOT) -L$(SYSROOT)lib -L$(SYSROOT)usr -L$(SYSROOT)usr/lib \
           -lliquid -liio -lad9361 -lc -lm -lfftw3 -lfftw3f -lini -lusb-1.0 -lserialport -lavahi-client -lavahi-common -lxml2 -lz -ldbus-1 -lfec -ltuntap

PLATFORM := $(shell uname -s)

-include Make.config


OUT_DIR := .build
SRC := $(foreach dir, $(DIRS), $(wildcard $(dir)/*$(SUFFIX)))
OBJ_ := $(SRC:$(SUFFIX)=.o)
OBJ := $(addprefix $(OUT_DIR)/,$(OBJ_))
DEPS := $(OBJ:.o=.d)
SHARED_SUFFIX := dll
STATIC_SUFFIX := lib

ifeq "$(PLATFORM)" "Linux"
    SHARED_SUFFIX := so
    STATIC_SUFFIX := a
endif

ifeq "$(LIBRARY)" "shared"
    OUT=lib$(NAME).$(SHARED_SUFFIX)
    LDFLAGS += -shared
else ifeq "$(LIBRARY)" "static"
    OUT=lib$(NAME).$(STATIC_SUFFIX)
else
    OUT=$(NAME)
endif

ifeq "$(SUFFIX)" ".cpp"
    COMPILER := $(CXX)
else ifeq "$(SUFFIX)" ".c"
    COMPILER := $(CROSS_COMPILE)gcc
endif

.SUFFIXES:
.PHONY: clean

$(OUT): $(OBJ)
ifeq "$(LIBRARY)" "static"
	@$(AR) rcs $@ $^
else
	@$(COMPILER) $^ $(LDFLAGS) -o $@
endif

$(OUT_DIR)/%.o: %$(SUFFIX)
	@mkdir -p $(dir $@)
	@$(COMPILER) $(CXXFLAGS) $(FLAGS) -MMD -MP -fPIC -c $< -o $@

clean:
	@$(RM) -r $(OUT) $(OUT_DIR)

-include: $(DEPS)
