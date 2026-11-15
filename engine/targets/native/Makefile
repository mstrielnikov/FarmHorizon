WORKSPACE_ROOT := ../../..
ENGINE_ROOT    := ../..
include $(ENGINE_ROOT)/mk/common.mk
include $(ENGINE_ROOT)/mk/native-toolchain.mk

PLATFORM := native
.DEFAULT_GOAL := all

# ── Raylib (bundled vcpkg if present, else host pkg-config / system libs) ──
VCPKG_LINUX := $(TOOLCHAIN)/vcpkg/installed/x64-linux
ifneq ($(wildcard $(VCPKG_LINUX)/include/raylib.h),)
  RAYLIB_INCLUDES := -I$(VCPKG_LINUX)/include -I$(TOOLCHAIN)/raylib-src/examples/core
  GUI_LIBS        := -L$(VCPKG_LINUX)/lib -lraylib -lglfw3 \
                     -l:libGL.so.1 -lm -lpthread -ldl -lrt \
                     -l:libX11.so.6 -l:libXrandr.so.2 -l:libXi.so.6 -l:libXcursor.so.1 -l:libXinerama.so.1
else
  RAYLIB_INCLUDES := $(shell pkg-config --cflags raylib 2>/dev/null) -I$(TOOLCHAIN)/raylib-src/examples/core
  GUI_LIBS        := $(or $(shell pkg-config --libs raylib 2>/dev/null),-lraylib -lm -lpthread -ldl)
endif

CXXFLAGS := $(CXXSTD) $(WARNINGS) -g $(TOOLCHAIN_FLAGS) $(INCLUDES) $(RAYLIB_INCLUDES)

include $(ENGINE_ROOT)/mk/lib.mk

# ── Executable ───────────────────────────────────
TARGET := $(BIN_DIR)/FarmHorizon-gui

all: $(TARGET)

$(TARGET): main.cpp $(LIB)
	@mkdir -p $(dir $@) $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(OBJ_DIR)/main.d main.cpp $(LIB) $(TOOLCHAIN_LDFLAGS) $(GUI_LIBS) -o $@

-include $(OBJ_DIR)/main.d

clean:
	rm -f $(TARGET)
	rm -rf $(OBJ_DIR)

.PHONY: all clean
