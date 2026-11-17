#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	$(shell basename $(CURDIR))
GAME_TITLE	:=	Cosmic Hex
GAME_SUBTITLE1	:=	A hex trading game in space
GAME_SUBTITLE2	:=	SAKBox
BUILD		:=	build
# The sources are staged from their own packages by the Bazel rule
# (extra_srcs in BUILD), as the 3DS's (../3ds) with the DS's scene backend
SOURCES		:=	cosmic_hex/handheld/source cosmic_hex/handheld/src cosmic_hex/dice/src cyclone/src cosmic_hex/rules/src cosmic_hex/art/src cosmic_hex/art scratchpadd/texture_recipe/src \
			scratchpadd/handheld_controls/src scratchpadd/utilities/src scratchpadd/storage/src scratchpadd_ds/platform/src \
			scratchpadd/input/src scratchpadd/debug_log/src/server scratchpadd/debug_log/src/session \
			scratchpadd/scene/src scratchpadd/scene_ds/src
INCLUDES	:=	include build cosmic_hex/handheld/include cosmic_hex/dice/include cyclone/include cosmic_hex/rules/include cosmic_hex/art/include scratchpadd/texture_recipe/include \
			action_log/include game_bots/include hex_grid/include seeded_random/include flexmath/include \
			scratchpadd/app_ended/include scratchpadd/handheld_controls/include scratchpadd/basis/include scratchpadd/messages/include scratchpadd/utilities/include \
			scratchpadd/input/include scratchpadd/debug_log/include scratchpadd/storage/include \
			scratchpadd/scene/include scratchpadd/scene_ds/include \
			third_party third_party/spdlog/include

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
# ARM instructions rather than devkitPro's usual Thumb: Thumb on the ARM9 has
# no 32 x 32 -> 64-bit multiply, which every fixed-point multiply needs, so
# each was a library call. ARM made the ragdoll's steps 2.8 times as fast.
ARCH	:=	-march=armv5te -mtune=arm946e-s -marm

CFLAGS	:=	-g -Wall -O2 -ffunction-sections -fdata-sections\
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9
# A debug build (bazel build -c dbg): see //projects/scratchpadd/debug_log
CFLAGS	+=	$(if $(SAKBOX_DEBUG),-DSAKBOX_DEBUG)
# The build's defines (//tools/devkitpro: the app's name and title, and a
# target's `defines`)
CFLAGS	+=	$(SAKBOX_DEFINES)
# Cyclone (the physics dice) in Q16.16 fixed point (flexmath's FlexPoint16,
# see precision.h), as the DS ragdoll: the DS has no floating-point
# hardware, so float was all done in software; fixed point is integer
# arithmetic, with the DS's hardware divider and square root
CFLAGS	+=	-DCYCLONE_FLEX_POINT16

# ScratchPadd needs RTTI (each message's type) and exceptions (a control of
# the wrong type), so unlike devkitPro's template this keeps both
CXXFLAGS	:= $(CFLAGS) -std=gnu++26

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -lfat -lnds9 # libfat: the SD card, for a debug build's log


#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(LIBNDS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(BINFILES:.bin=.o) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).ds.gba


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).nds	: 	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES) hot_in_itcm.stamp

#---------------------------------------------------------------------------------
# Code marked [[gnu::hot]] (Cyclone's resolver: its work each sub-step) runs
# from ITCM, the ARM9's 32 KB of fast instruction memory: before linking,
# each object's hot sections (.text.hot.*) are renamed .itcm, which
# devkitPro's linker script places there. From main RAM, which is slow to
# read, the hot code is more than the instruction cache holds.
#---------------------------------------------------------------------------------
hot_in_itcm.stamp	:	$(OFILES)
	@for object in $(OFILES); do \
		renames=$$($(PREFIX)objdump -h $$object | \
			awk '$$2 ~ /^\.text\.hot\./ { printf " --rename-section %s=.itcm", $$2 }'); \
		[ -z "$$renames" ] || $(OBJCOPY) $$renames $$object; \
	done
	@touch $@

#---------------------------------------------------------------------------------
%.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)


-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
