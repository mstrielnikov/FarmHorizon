WORKSPACE_ROOT := ../..
include $(WORKSPACE_ROOT)/common.mk
include $(WORKSPACE_ROOT)/native-toolchain.mk

PLATFORM := native
.DEFAULT_GOAL := all

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
