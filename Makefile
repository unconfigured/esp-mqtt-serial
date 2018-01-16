TOOLDIR := /opt/esp-open-sdk/xtensa-lx106-elf/bin
SDKLIBDIR    := /opt/esp-open-sdk/ESP8266_NONOS_SDK_V2.0.0_16_08_10/lib
ESPTOOL := $(TOOLDIR)/esptool.py

BIN = "esp-mqtt-serial"
SRCDIR = ./user ./esp_mqtt/mqtt
#SUBDIRS = ./esp_mqtt/mqtt
SDK_LIBS = c gcc main net80211 crypto ssl wpa lwip pp phy

SOURCES  := $(wildcard $(addsuffix /*c, $(SRCDIR)))
EXTRA_INCLUDES := -I$(SDKLIBDIR)/../driver_lib/include
INCLUDES := $(foreach d, $(SRCDIR), -I$d -I$d/include)
OBJECTS  := $(SOURCES:%.c=%.o)
SDK_LIBS := $(addprefix -l,$(SDK_LIBS))
LDDIR    := /opt/esp-open-sdk/ESP8266_NONOS_SDK_V2.0.0_16_08_10/ld
LDFILE   := eagle.app.v6.old.1024.app1.ld
#LDFILE   := eagle.app.v6.ld

#CFLAGS = -Os -DICACHE_FLASH -mlongcalls 
#EXTRA_CFLAGS = -DMQTT_DEBUG_ON -DDEBUG_ON
CFLAGS += \
    -Wpointer-arith \
    -Wundef \
    -Werror \
    -Wl,-EL \
    -fno-inline-functions \
    -nostdlib \
    -mlongcalls \
    -mtext-section-literals \
    -ffunction-sections \
    -fdata-sections \
    -fno-builtin-print \
	-DICACHE_FLASH \
	$(EXTRA_CFLAGS)

LDFLAGS = \
	-nostdlib \
	-L$(SDKLIBDIR) \
	-L$(LDDIR) \
    -T$(LDFILE) \
    -u call_user_start \
    -Wl,-static \
	-Wl,--start-group \
        $(SDK_LIBS) \
    -Wl,--end-group

CC		:= $(TOOLDIR)/xtensa-lx106-elf-gcc
AR		:= $(TOOLDIR)/xtensa-lx106-elf-ar
LD		:= $(CC)
NM      := $(TOOLDIR)/xtensa-lx106-elf-nm
CPP     := $(TOOLDIR)/xtensa-lx106-elf-cpp
OBJCOPY := $(TOOLDIR)/xtensa-lx106-elf-objcopy
OBJDUMP := $(TOOLDIR)/xtensa-lx106-elf-objdump

export PATH := $(TOOLDIR):$(PATH)

.PHONY: flash clean subdirs $(SUBDIRS)

all: subdirs $(BIN).bin

$(BIN).bin: $(BIN)
	$(ESPTOOL) elf2image $^

$(BIN): $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@

$(OBJECTS): %.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(EXTRA_INCLUDES) -c $< -o $@

flash: $(BIN).bin
	$(ESPTOOL) write_flash 0 esp-mqtt-serial-0x00000.bin 0x11000 esp-mqtt-serial-0x11000.bin

clean:
	rm -f $(OBJECTS) esp-mqtt-serial esp-mqtt-serial-0x00000.bin esp-mqtt-serial-0x11000.bin

