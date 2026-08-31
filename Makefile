##############################################################################
# NanoAnalyzer build - NanoVNA-H4 (STM32F303) only
##############################################################################

# Compiler options
ifeq ($(USE_OPT),)
USE_OPT = -O2 -fno-inline-small-functions -ggdb -fomit-frame-pointer -falign-functions=16 --specs=nano.specs -fstack-usage -std=c11
endif
USE_OPT+= -ffast-math -fsingle-precision-constant
USE_OPT+= -fno-reorder-blocks

ifeq ($(USE_COPT),)
  USE_COPT =
endif
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif
ifeq ($(USE_LDOPT),)
  USE_LDOPT =
endif
ifeq ($(USE_LTO),)
  USE_LTO = no
endif
ifeq ($(USE_THUMB),)
  USE_THUMB = yes
endif
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

ifeq ($(VERSION),)
  VERSION="$(shell git describe --tags 2>/dev/null || echo v0)"
endif

##############################################################################
# Architecture / project options
#
USE_FPU = hard

ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x200
endif
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x100
endif

##############################################################################
# Project, sources and paths
#
PROJECT = H4
CHIBIOS = ChibiOS
PROJ = .

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f3xx.mk
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F3xx/platform.mk
include NANOVNA_STM32_F303/board.mk
include $(CHIBIOS)/os/hal/osal/rt/osal.mk
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/port_v7m.mk
include $(CHIBIOS)/os/hal/lib/streams/streams.mk

LDSCRIPT = NANOVNA_STM32_F303/STM32F303xC.ld

CSRC = $(STARTUPSRC) \
       $(KERNSRC) \
       $(PORTSRC) \
       $(OSALSRC) \
       $(HALSRC) \
       $(PLATFORMSRC) \
       $(BOARDSRC) \
       $(STREAMSSRC) \
       FatFs/ff.c \
       FatFs/ffunicode.c \
       fonts/numfont16x22.c \
       fonts/Font5x7.c \
       fonts/Font6x10.c \
       fonts/Font7x11b.c \
       fonts/Font11x14.c \
       usbcfg.c \
       main.c common.c si5351.c tlv320aic3204.c dsp.c plot.c ui.c lcd.c data_storage.c hardware.c vna_math.c bands.c

CPPSRC =
ACSRC =
ACPPSRC =
TCSRC =
TCPPSRC =
ASMSRC = $(STARTUPASM) $(PORTASM) $(OSALASM)

INCDIR = $(STARTUPINC) $(KERNINC) $(PORTINC) $(OSALINC) \
         $(HALINC) $(PLATFORMINC) $(BOARDINC) \
         $(STREAMSINC)

##############################################################################
# Compiler settings
#
MCU  = cortex-m4

TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
LD   = $(TRGT)gcc
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
AR   = $(TRGT)ar
OD   = $(TRGT)objdump
SZ   = $(TRGT)size
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary
ELF  = $(CP) -O elf

AOPT =
TOPT = -mthumb -DTHUMB
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes
CPPWARN = -Wall -Wextra -Wundef

##############################################################################
# User defines
#
UDEFS = -DARM_MATH_CM4 -DVERSION=\"$(VERSION)\" -DNANOVNA_F303
UDEFS+= -DVNA_AUTO_SELECT_RTC_SOURCE

UADEFS =
UINCDIR =
ULIBDIR =
ULIBS = -lm

RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC
include $(RULESPATH)/rules.mk

flash: build/$(PROJECT).bin
	dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D build/$(PROJECT).bin
