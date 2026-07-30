# Copyright (c) 2026 Alex313031

# templed Makefile for gcc

SRCDIR  := src
TARGET  := templed

OBJECTS := templed.o gpio.o sensors.o utils.o
HEADERS := $(SRCDIR)/templed.h $(SRCDIR)/gpio.h $(SRCDIR)/sensors.h $(SRCDIR)/utils.h \
           $(SRCDIR)/pch.h

# WiringPi is vendored as a git submodule with its own Makefile-based
# build; we build its static archive (fits our -static linking) and link
# it directly. Its own link needs go on our final link line below
WIRINGPI_DIR := wiringpi/wiringPi
WIRINGPI_LIB := $(WIRINGPI_DIR)/libwiringPi.a

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

ifeq ($(IS_DEBUG),1)
  CPPFLAGS += -DDEBUG -D_DEBUG
  CFLAGS   += -Wall -Og -g2
  CXXFLAGS += -std=c++17
  LDFLAGS  += -static
else
  CPPFLAGS += -DNDEBUG -D_NDEBUG
  CFLAGS   += -Wno-error -O2 -g0
  CXXFLAGS += -std=c++17
  LDFLAGS  += -static -s
endif

all: $(TARGET)

$(TARGET): $(OBJECTS) $(WIRINGPI_LIB)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(WIRINGPI_LIB) -lm -lpthread -lrt

$(WIRINGPI_LIB):
	$(MAKE) -C $(WIRINGPI_DIR) static

# Pattern rule: each foo.o is built from its matching src/foo.cc; `$<` is
# the first prerequisite, i.e. the one .cc file the % stem selected
%.o: $(SRCDIR)/%.cc $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJECTS)
	-$(MAKE) -C $(WIRINGPI_DIR) clean

.PHONY: all clean
