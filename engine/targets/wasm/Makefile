WORKSPACE_ROOT := ../../..
ENGINE_ROOT    := ../..
include $(ENGINE_ROOT)/mk/common.mk

PLATFORM := web
.DEFAULT_GOAL := all

# ── EMSDK Sandbox (.toolchain/ if present, else host PATH) ──
ifneq ($(wildcard $(TOOLCHAIN)/emsdk/upstream/emscripten/em++),)
  EMSDK_DIR  := $(abspath $(TOOLCHAIN)/emsdk)
  EMCC_DIR   := $(EMSDK_DIR)/upstream/emscripten
  export PATH  := $(EMCC_DIR):$(EMSDK_DIR):$(PATH)
  export EMSDK := $(EMSDK_DIR)
endif
ifneq ($(wildcard $(TOOLCHAIN)/python/bin/python3.11),)
  PYTHON_BIN := $(abspath $(TOOLCHAIN)/python/bin)
  export PATH         := $(PYTHON_BIN):$(PATH)
  export EMSDK_PYTHON := $(PYTHON_BIN)/python3.11
endif

CXX := em++
AR  := emar

# ── Raylib (built from source for PLATFORM_WEB) ──
RAYLIB_SRC      := $(TOOLCHAIN)/raylib-src/src
RAYLIB_LIB      := $(RAYLIB_SRC)/libraylib.web.a
RAYLIB_INCLUDES := -I$(RAYLIB_SRC) -I$(RAYLIB_SRC)/external -I$(TOOLCHAIN)/raylib-src/examples/core

CXXFLAGS := $(CXXSTD) $(WARNINGS) -Os $(INCLUDES) $(RAYLIB_INCLUDES) -DPLATFORM_WEB

include $(ENGINE_ROOT)/mk/lib.mk

# ── Emscripten Linker ────────────────────────────
# Builds both the modular ES6 build (farmhorizon.js + farmhorizon.wasm) and the
# single-file ES6 build (farmhorizon-single.js) consumed by the anywidget bundle.
WIDGET_EXPORTS := _main,_SetExternalSceneState,_SetTimeOfDay,_SetWidgetSize,_SetWidgetOptions,_SetBackgroundColor

LDFLAGS := -s USE_GLFW=3 \
           -s ASYNCIFY \
           -s INITIAL_MEMORY=32MB \
           -s ALLOW_MEMORY_GROWTH=1 \
           -s MODULARIZE=1 \
           -s EXPORT_ES6=1 \
           -s EXPORT_NAME=createFarmHorizon \
           -s ENVIRONMENT=web \
           -s EXPORTED_FUNCTIONS=$(WIDGET_EXPORTS) \
           -s EXPORTED_RUNTIME_METHODS=cwrap,ccall,specialHTMLTargets

MAIN_OBJ  := $(OBJ_DIR)/main.o
TARGET    := $(WASM_DIR)/farmhorizon.js
SINGLE_JS := $(WASM_DIR)/farmhorizon-single.js

all: $(TARGET) $(SINGLE_JS)

$(MAIN_OBJ): main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB) $(LDFLAGS) -o $@

$(SINGLE_JS): $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB) $(LDFLAGS) -s SINGLE_FILE=1 -o $@

-include $(OBJ_DIR)/main.d

$(RAYLIB_LIB):
	@echo "Building Raylib for WebAssembly..."
	@test -d "$(RAYLIB_SRC)" || { echo "Error: $(RAYLIB_SRC) not found. Run 'make setup-wasm' from project root."; exit 1; }
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_WEB -j$$(nproc)

clean:
	rm -f $(WASM_DIR)/farmhorizon.js $(WASM_DIR)/farmhorizon.wasm $(SINGLE_JS)
	rm -rf $(OBJ_DIR)

clean-all: clean
	$(MAKE) -C $(RAYLIB_SRC) clean 2>/dev/null || true

.PHONY: all clean clean-all
