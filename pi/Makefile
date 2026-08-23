#
# BMC-K4510 on a Raspberry Pi 3B+, bare metal: Circle + circle-libsdl2.
# The frontend is sdl/main.c, unchanged; this directory supplies the Circle
# kernel (three cores), the host glue, and the C64 keyboard on GPIO. Files
# opened from the emulator core go through the library's I/O service
# (circle-syscallwrap redirects the C library's open/read/opendir/...).
#
#   make BOARD=rpi3 SHIM=/path/to/circle-libsdl2
#
BOARD ?= rpi3
SHIM  ?= $(HOME)/Projects/k4510-pi/circle-libsdl2
CIRCLESTDLIBHOME ?= $(SHIM)/circle-stdlib-$(BOARD)

include $(CIRCLESTDLIBHOME)/Config.mk

CORE = ../core
RESID_SRC = $(wildcard $(CORE)/resid/*.cc)
OBJS = main.o kernel.o host_circle.o c64kbd.o frontend.o \
       $(CORE)/xemu/cpu65.o $(CORE)/mem.o $(CORE)/io.o $(CORE)/vicke.o $(CORE)/sid.o \
       $(RESID_SRC:.cc=.o)

STANDARD = -std=c++23 -Wno-volatile
C_STANDARD = -std=gnu11

include $(SHIM)/circle-syscallwrap/syscallwrap.mk
include $(CIRCLEHOME)/Rules.mk

INCLUDE := -I $(SHIM)/include $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) -I $(CORE)
CFLAGS  += -DK4510_PI -Wno-unused-function
LIBS := $(SHIM)/libSDL2-$(BOARD).a $(CIRCLE_STDLIB_LIBS)

# the desktop frontend, compiled here under another name
frontend.o: ../sdl/main.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(C_STANDARD) -I $(SHIM)/include/SDL2 -c -o $@ $<

# reSID is .cc; Circle's rules only know .cpp
%.o: %.cc
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -DVERSION=\"0.16\" -Wno-unused-parameter -c -o $@ $<

-include $(DEPS)

include $(SHIM)/sdl-app.mk

# syscallwrap.mk comes before Rules.mk and would otherwise be the default goal
.DEFAULT_GOAL := kernel8.img
