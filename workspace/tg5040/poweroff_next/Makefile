###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../platform/Makefile.env

###########################################################

TARGET = poweroff_next
INCDIR = -I. -I../../all/common -I../platform
SOURCE = ../../all/poweroff_next/$(TARGET).c ../../all/common/utils.c ../../all/common/config.c

CC = $(CROSS_COMPILE)gcc
# Override CFLAGS and LDFLAGS to not include SDL2 dependencies
CFLAGS = $(OPT) -g -std=gnu11 -Wall -Wextra
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\"
# tg5040 cuts power at the AXP2202 PMIC over I2C (limbo-bug workaround)
CFLAGS += -DHAS_AXP2202_POWEROFF
LDFLAGS = -lpthread -ldl

PRODUCT = build/$(PLATFORM)/$(TARGET).elf

all:
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCT)
