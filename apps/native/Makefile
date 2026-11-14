WORKSPACE_ROOT := ../..
include $(WORKSPACE_ROOT)/common.mk

PLATFORM := native
.DEFAULT_GOAL := all

# ── Toolchain ────────────────────────────────────
# Prefer the bundled LLVM toolchain; fall back to emsdk's clang against the system libstdc++.
ifneq ($(wildcard $(WORKSPACE_ROOT)/.toolchain/llvm/bin/clang++),)
  CXX               := $(WORKSPACE_ROOT)/.toolchain/llvm/bin/clang++
  AR                := $(WORKSPACE_ROOT)/.toolchain/llvm/bin/llvm-ar
  TOOLCHAIN_FLAGS   := -stdlib=libc++
  TOOLCHAIN_LDFLAGS := -fuse-ld=lld -stdlib=libc++ -Wl,-rpath,'$$ORIGIN/../.toolchain/llvm/lib/x86_64-unknown-linux-gnu'
else
  CXX               := $(WORKSPACE_ROOT)/emsdk/upstream/bin/clang++
  AR                := $(WORKSPACE_ROOT)/emsdk/upstream/bin/llvm-ar
  TOOLCHAIN_FLAGS   := --gcc-toolchain=/usr
  TOOLCHAIN_LDFLAGS :=
endif

# ── Raylib (vcpkg) ───────────────────────────────
RAYLIB_PATH     := $(WORKSPACE_ROOT)/vcpkg/installed/x64-linux
RAYLIB_INCLUDES := -I$(RAYLIB_PATH)/include -I$(WORKSPACE_ROOT)/raylib-src/examples/core
GUI_LIBS        := -L$(RAYLIB_PATH)/lib -lraylib -lglfw3 \
                   -l:libGL.so.1 -lm -lpthread -ldl -lrt \
                   -l:libX11.so.6 -l:libXrandr.so.2 -l:libXi.so.6 -l:libXcursor.so.1 -l:libXinerama.so.1

CXXFLAGS := $(CXXSTD) $(WARNINGS) -g $(TOOLCHAIN_FLAGS) $(INCLUDES) $(RAYLIB_INCLUDES)

include $(WORKSPACE_ROOT)/lib.mk

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
