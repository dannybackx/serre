#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# Try to avoid warnings about deprecated declarations
CFLAGS += -Wno-deprecated-declarations
CXXFLAGS += -Wno-deprecated-declarations

#
# Derive numeric version macros to pass to both the C and C++ preprocessor
#
MY_IDF_VER := $(shell cd ${IDF_PATH} && git describe --always --tags --dirty)
MY_CFLAGS = $(shell echo ${MY_IDF_VER} | awk -f ${COMPONENT_PATH}/idf-version.awk)
CFLAGS += ${MY_CFLAGS}
CXXFLAGS += ${MY_CFLAGS}

COMPONENT_SRCDIRS += ../libraries/FastLED

COMPONENT_ADD_INCLUDEDIRS := ../libraries/FastLED .

COMPONENT_EMBED_TXTFILES :=  ${PROJECT_PATH}/dannybackx.hopto.org.crt

#
# These lines were copied from make/component_wrapper.mk in the esp-idf distro
# Obviously renamed COMPONENT_OBJS to MY_COMPONENT_OBJS
#
# Currently a copy from the v3.1.3 version
#
MY_COMPONENT_OBJS := $(foreach compsrcdir,$(COMPONENT_SRCDIRS),$(patsubst %.c,%.o,$(wildcard $(COMPONENT_PATH)/$(compsrcdir)/*.c)))
MY_COMPONENT_OBJS += $(foreach compsrcdir,$(COMPONENT_SRCDIRS),$(patsubst %.cpp,%.o,$(wildcard $(COMPONENT_PATH)/$(compsrcdir)/*.cpp)))
MY_COMPONENT_OBJS += $(foreach compsrcdir,$(COMPONENT_SRCDIRS),$(patsubst %.cc,%.o,$(wildcard $(COMPONENT_PATH)/$(compsrcdir)/*.cc)))
MY_COMPONENT_OBJS += $(foreach compsrcdir,$(COMPONENT_SRCDIRS),$(patsubst %.S,%.o,$(wildcard $(COMPONENT_PATH)/$(compsrcdir)/*.S)))
# Make relative by removing COMPONENT_PATH from all found object paths
MY_COMPONENT_OBJS := $(patsubst $(COMPONENT_PATH)/%,%,$(MY_COMPONENT_OBJS))
MY_COMPONENT_OBJS := $(call stripLeadingParentDirs,$(MY_COMPONENT_OBJS))
MY_COMPONENT_OBJS := $(foreach obj,$(MY_COMPONENT_OBJS),$(if $(filter $(abspath $(obj)),$(abspath $(COMPONENT_OBJEXCLUDE))), ,$(obj)))
MY_COMPONENT_OBJS := $(call uniq,$(MY_COMPONENT_OBJS))

COMPONENT_EXTRA_CLEAN := build.h

build.h:	${MY_COMPONENT_OBJS}
	echo "Regenerating build timestamp .."
	echo -n '#define __BUILD__ "' >build.h
	echo -n `date '+%Y/%m/%d %T'` >>build.h
	echo '"' >>build.h

$(COMPONENT_LIBRARY):	$(COMPONENT_BUILD_DIR)/build_date.o

build_date.o: build.h
