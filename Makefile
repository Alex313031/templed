# Copyright (c) 2026 Alex313031

# templed Makefile for gcc

SRCDIR  := src
TARGET  := templed

OBJECTS := templed.o gpio.o sensors.o utils.o
HEADERS := $(SRCDIR)/templed.h $(SRCDIR)/gpio.h $(SRCDIR)/sensors.h $(SRCDIR)/utils.h \
           $(SRCDIR)/pch.h

# WiringPi is vendored as a git submodule with its own Makefile-based
# build. It is LGPL v3, and upstream refuses to build a static archive
# (the `static` target just prints a notice) - statically linking an LGPL
# library into a distributed binary would obligate shipping relinkable
# objects. So: build the shared library, symlink the unversioned name the
# linker looks for, and set an $ORIGIN rpath - $ORIGIN means "the
# directory the binary itself is in", so the loader finds the .so next to
# a shipped binary, or under wiringpi/ when run from this repo. Fully
# relocatable: no absolute paths baked in. `make dist` stages the
# shippable pair
WIRINGPI_DIR := wiringpi/wiringPi
WIRINGPI_LIB := $(WIRINGPI_DIR)/libwiringPi.so
WIRINGPI_VER := $(shell cat wiringpi/VERSION)

CPPFLAGS += -I$(WIRINGPI_DIR)

# Compiler toolchain defaults
ifeq ($(USE_LLVM),1)
  CC   := clang
  CXX  := clang++
  AR   := llvm-ar
  LD   := lld
else
  CC   := gcc
  CXX  := g++
  AR   := ar
  LD   := ld
endif

# Note: no full -static here, unlike raspimon - the vendored WiringPi is
# LGPL and shared-library only (see the WiringPi section below)
ifeq ($(IS_DEBUG),1)
  CPPFLAGS += -DDEBUG -D_DEBUG
  CFLAGS   += -Wall -Og -g2
  CXXFLAGS += -std=c++17
else
  CPPFLAGS += -DNDEBUG -D_NDEBUG
  CFLAGS   += -Wno-error -O2 -g0
  CXXFLAGS += -std=c++17
  LDFLAGS  += -s
endif

# The C++ runtime IS linked statically, so the shipped pair (templed +
# libwiringPi.so) carries no libstdc++/libgcc version dependency; only
# glibc stays dynamic, which any same-or-newer Raspberry Pi OS satisfies
LDFLAGS += -static-libstdc++ -static-libgcc

all: $(TARGET)

$(TARGET): $(OBJECTS) $(WIRINGPI_LIB)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -L$(WIRINGPI_DIR) -lwiringPi \
	  -Wl,-rpath,'$$ORIGIN' -Wl,-rpath,'$$ORIGIN/$(WIRINGPI_DIR)' -lm

$(WIRINGPI_LIB):
	$(MAKE) -C $(WIRINGPI_DIR) all
	ln -sf libwiringPi.so.$(WIRINGPI_VER) $(WIRINGPI_LIB)

# Stage the relocatable two-file bundle: the binary plus the WiringPi
# shared library, the latter under the exact soname the loader searches
# for. Copy the dist/ contents anywhere together and templed just runs
dist: $(TARGET)
	mkdir -p dist
	cp $(TARGET) dist/
	cp $(WIRINGPI_DIR)/libwiringPi.so.$(WIRINGPI_VER) dist/libwiringPi.so

# Pattern rule: each foo.o is built from its matching src/foo.cc; `$<` is
# the first prerequisite, i.e. the one .cc file the % stem selected
%.o: $(SRCDIR)/%.cc $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJECTS)
	$(RM) -r dist
	-$(MAKE) -C $(WIRINGPI_DIR) clean

.PHONY: all clean dist
