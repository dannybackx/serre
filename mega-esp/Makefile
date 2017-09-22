#====================================================================================
#
# A makefile for Arduino projects.
# This is based on work by Peter Lerup, but heavily modified.
#
# License: GPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2016 Peter Lerup. All rights reserved.
#
#====================================================================================
#====================================================================================
# User editable area
#====================================================================================

SKETCH=		$(HOME)/src/sketchbook/mega-esp/kippen-mega/kippen.ino
#EXTRA_DEFINES=	-DBUILT_BY_MAKE
EXTRA_SRC=	personal.c Hatch.cpp ThingSpeak.cpp SFE_BMP180.cpp \
		callback.cpp strings.c secrets.c Ifttt.cpp Light.cpp Dyndns.cpp \
		Sunset.cpp
# EXTRA_SRC +=	AFMotor.cpp
EXTRA_SRC +=	SimpleL298.cpp

#UPLOAD_HOST=	testesp
PRODUCTION_HOST=	kippen
UPLOAD_HOST=	d1mini3

UPLOAD_PORT=	/dev/ttyACM0

UPLOAD_WIRING=	wiring
#UPLOAD_WIRING=	arduino
UPLOAD_MCU=	m2560
UPLOAD_BAUD=	115200

BUILD_ROOT=	tmp
LIBCPP=

AVR_MYLIBS = $(HOME)/src/sketchbook/libraries
AVR_MEGALIBS = $(HOME)/src/sketchbook/mega-esp/libraries

LIBS =	\
       ${AVR_MEGALIBS}/ELClient \
       ${AVR_LIBS}/Wire \
       ${AVR_MYLIBS}/Time \
       ${AVR_MYLIBS}/DS1307RTC

#       ${AVR_MYLIBS}/ArduinoWiFi \
#=== Project specific definitions: sketch and list of needed libraries

#====================================================================================
# The area below should normally not need to be edited
#====================================================================================

ARDUINO_ROOT=	/home/danny/src/arduino/arduino-1.7.11-linux64
AVR_ROOT=	${ARDUINO_ROOT}/hardware/arduino/avr
AVR_LIBS=	$(AVR_ROOT)/libraries
# AVRFLASH=	${ARDUINO_ROOT}/hardware/tools/esp-link-tools/avrflash
AVRFLASH=	${HOME}/src/github/jeelabs/esp-link/megaflash
ARCH=		avr

time_string = $(shell perl -e 'use POSIX qw(strftime); print strftime($(1), localtime());')

START_TIME := $(shell perl -e "print time();")
# Main output definitions
MAIN_NAME = $(basename $(notdir $(SKETCH)))
MAIN_EXE = $(BUILD_ROOT)/$(MAIN_NAME).bin
MAIN_ELF = $(OBJ_DIR)/$(MAIN_NAME).elf
MAIN_EEP = $(OBJ_DIR)/$(MAIN_NAME).eep
MAIN_HEX = $(OBJ_DIR)/$(MAIN_NAME).hex
SRC_GIT_VERSION =

# Directory for intermedite build files
OBJ_DIR = $(BUILD_ROOT)/obj
OBJ_EXT = .o
DEP_EXT = .d

# Compiler definitions
CC = ${ARCH}-gcc
CPP = ${ARCH}-g++
LD =  $(CC)
AR = ${ARCH}-ar
AVR_TOOL = /home/danny/.arduino15/packages/esp8266/tools/esptool/0.4.6/esptool

#DEBUG=-g
DEBUG=-Os -g

#MCU=	atmega328p
MCU=	atmega2560
#VARIANT=	standard
VARIANT=	mega

INCLUDE_DIRS +=	${CORE_DIR} ${AVR_ROOT}/variants/${VARIANT} ${OBJ_DIR}
C_DEFINES=	-c ${DEBUG} -w -MMD \
		-ffunction-sections -fdata-sections -mmcu=${MCU} \
		-DF_CPU=16000000L -DARDUINO=107011 -DARDUINO_ARCH_AVR \
		${EXTRA_DEFINES} 
C_INCLUDES=	$(foreach dir,$(INCLUDE_DIRS) $(USER_DIRS),-I$(dir))
C_FLAGS ?=	-c ${DEBUG} -std=gnu11 -ffunction-sections -fdata-sections -MMD -mmcu=${MCU}
CPP_FLAGS ?=	-c ${DEBUG} -std=gnu++11 \
		-fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics \
		-MMD -mmcu=${MCU}
S_FLAGS ?=	-c ${DEBUG} -x assembler-with-cpp -MMD
LD_FLAGS ?=	-w ${DEBUG} -Wl,--gc-sections -mmcu=${MCU}
LD_STD_LIBS ?=	-lm

# Core source files
CORE_DIR = $(AVR_ROOT)/cores/arduino
CORE_SRC = $(shell find $(CORE_DIR) -name "*.S" -o -name "*.c" -o -name "*.cpp")
CORE_OBJ = $(patsubst %,$(OBJ_DIR)/%$(OBJ_EXT),$(notdir $(CORE_SRC)))
CORE_LIB = $(OBJ_DIR)/core.ar

# User defined compilation units
USER_SRC = $(SKETCH) ${EXTRA_SRC} $(shell find $(LIBS) -name "*.cpp" -o -name "*.c")
USER_INCL = $(SKETCH) ${EXTRA_SRC} $(shell find $(LIBS) -name "*.h")
# Object file suffix seems to be significant for the linker...
USER_OBJ = $(subst .ino,.cpp,$(patsubst %,$(OBJ_DIR)/%$(OBJ_EXT),$(notdir $(USER_SRC))))
USER_DIRS = $(sort $(dir $(USER_SRC) $(USER_INCL)))

VPATH += $(shell find $(CORE_DIR) -type d) $(USER_DIRS)

# Automatically generated build information data
# Makes the build date and git descriptions at the actual build
# event available as string constants in the program
BUILD_INFO_H = $(OBJ_DIR)/buildinfo.h
BUILD_INFO_CPP = $(OBJ_DIR)/buildinfo.cpp
BUILD_INFO_OBJ = $(BUILD_INFO_CPP)$(OBJ_EXT)

# Temp files
VI_TMP1=	${USER_SRC:.c=.c~}
VI_TMP2=	${VI_TMP1:.ino=.ino~}
VI_TMP3=	${VI_TMP2:.cpp=.cpp~}
VI_TMPFILES=	${VI_TMP3:.h=.h~}
BAK_FILES=

$(BUILD_INFO_H): | $(OBJ_DIR)
	echo "typedef struct { const char *date, *time, *src_filename, *src_version, *env_version;} _tBuildInfo; extern _tBuildInfo _BuildInfo;" >$@


# Build rules
$(OBJ_DIR)/%.cpp$(OBJ_EXT): %.cpp $(BUILD_INFO_H)
	echo C++-Compile $(<F)
	$(CPP) $(C_DEFINES) $(C_INCLUDES) $(CPP_FLAGS) $< -o $@

$(OBJ_DIR)/%.cpp$(OBJ_EXT): %.ino $(BUILD_INFO_H)
	echo C++-Compile $(<F)
	$(CPP) -x c++ -include $(CORE_DIR)/Arduino.h $(C_DEFINES) $(C_INCLUDES) $(CPP_FLAGS) $< -o $@

$(OBJ_DIR)/%.c$(OBJ_EXT): %.c
	echo Compile $(<F)
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) $< -o $@

$(OBJ_DIR)/%.S$(OBJ_EXT): %.S
	echo Compile $(<F)
	$(CC) $(C_DEFINES) $(C_INCLUDES) $(S_FLAGS) $< -o $@

$(CORE_LIB): $(CORE_OBJ)
	echo  Creating core archive
	rm -f $@
	$(AR) cru $@  $^

BUILD_DATE = $(call time_string,"%Y-%m-%d")
BUILD_TIME = $(call time_string,"%H:%M:%S")

$(MAIN_EXE): $(CORE_LIB) $(USER_OBJ)
	echo Linking $(MAIN_EXE)
	echo 	'#include <buildinfo.h>' >$(BUILD_INFO_CPP)
	echo '_tBuildInfo _BuildInfo = {"$(BUILD_DATE)","$(BUILD_TIME)","$(SKETCH)","",""};' >>$(BUILD_INFO_CPP)
	$(CPP) $(C_DEFINES) $(C_INCLUDES) $(CPP_FLAGS) $(BUILD_INFO_CPP) -o $(BUILD_INFO_OBJ)
	$(LD) $(LD_FLAGS) -Wl,--start-group $^ $(BUILD_INFO_OBJ) $(LD_STD_LIBS) -Wl,--end-group -L$(OBJ_DIR) -o $(MAIN_ELF)
	${ARCH}-objcopy -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0  ${MAIN_ELF} ${MAIN_EEP}
	${ARCH}-objcopy -O ihex -R .eeprom ${MAIN_ELF} ${MAIN_HEX}
	${ARCH}-size ${MAIN_ELF}
	perl -e 'print "Build complete. Elapsed time: ", time()-$(START_TIME),  " seconds\n\n"'

upload: all
	avrdude -C${ARDUINO_ROOT}/hardware/tools/avr/etc/avrdude.conf -v true -p${UPLOAD_MCU} -c${UPLOAD_WIRING} -P${UPLOAD_PORT} -b${UPLOAD_BAUD} -D -Uflash:w:${MAIN_HEX}:i

ota: all
	$(AVRFLASH) $(UPLOAD_HOST) $(MAIN_HEX)

pota: all
	$(AVRFLASH) $(PRODUCTION_HOST) $(MAIN_HEX)

showotaip:
	echo "IP Address for OTA is $(UPLOAD_AVAHI_IP) (symbol $(UPLOAD_AVAHI_NAME))"

objdump:	${MAIN_ELF}
	${ARCH}-objdump -xSsd ${MAIN_ELF} >$@

clean::
	echo Removing all intermediate build files...
	-rm -f ${VI_TMPFILES} ${BAK_FILES}
	-rm -f $(OBJ_DIR)/*
	-rm -rf ${BUILD_ROOT}
	-rm -f objdump

clean::
	-rm -f ${LINKS} links

all:: ${LINKS}

${LINKS}::	links

links::	${LINKS:%=../serre/%}
	-rm -f ${LINKS}
	for i in ${LINKS}; do  ln -s ../serre/$$i .; done
	touch links

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

.PHONY: all
all:: $(OBJ_DIR) $(BUILD_INFO_H) $(MAIN_EXE)

# Include all available dependencies
-include $(wildcard $(OBJ_DIR)/*$(DEP_EXT))

.DEFAULT_GOAL = all

ifndef VERBOSE
# Set silent mode as default
MAKEFLAGS += --silent
endif
