WORKSPACE_ROOT := ../..
include $(WORKSPACE_ROOT)/common.mk

PLATFORM := web
.DEFAULT_GOAL := all

# ── EMSDK Sandbox ────────────────────────────────
# All toolchain deps live inside the project directory.
# No system-wide installs required — run `make setup-wasm` from root to bootstrap.

PYTHON_BIN  := $(abspath $(WORKSPACE_ROOT)/python/bin)
EMSDK_DIR   := $(abspath $(WORKSPACE_ROOT)/emsdk)
EMCC_DIR    := $(EMSDK_DIR)/upstream/emscripten

export PATH            := $(PYTHON_BIN):$(EMCC_DIR):$(EMSDK_DIR):$(PATH)
export EMSDK_PYTHON    := $(PYTHON_BIN)/python3.11
export EMSDK           := $(EMSDK_DIR)

# ── Compiler ─────────────────────────────────────

CXX := em++
AR  := emar

# ── Raylib (built from source for PLATFORM_WEB) ──

RAYLIB_SRC      := $(WORKSPACE_ROOT)/raylib-src/src
RAYLIB_LIB      := $(RAYLIB_SRC)/libraylib.web.a
RAYLIB_INCLUDES := -I$(RAYLIB_SRC) -I$(RAYLIB_SRC)/external -I$(WORKSPACE_ROOT)/raylib-src/examples/core

CXXFLAGS := $(CXXSTD) $(WARNINGS) -Os $(INCLUDES) $(RAYLIB_INCLUDES) -DPLATFORM_WEB

include $(WORKSPACE_ROOT)/lib.mk

# ── Emscripten Linker ────────────────────────────
# Builds an ES module (createFarmHorizon) that a page instantiates on its own
# canvas, instead of a full-page HTML shell. ASYNCIFY stays: raylib's web
# backend calls emscripten_sleep.

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

# ── Output ───────────────────────────────────────

OUT_DIR      := $(WORKSPACE_ROOT)/build-wasm
MAIN_OBJ     := $(OBJ_DIR)/main.o
TARGET       := $(OUT_DIR)/farmhorizon.js
SINGLE_JS    := $(OUT_DIR)/farmhorizon-single.js

# TypeScript widget wrapper, anywidget bundle + demo page, bundled with Bun.
BUN          ?= bun
DEMO_TS      := $(wildcard demo/*.ts)
WIDGET_JS    := $(OUT_DIR)/farmhorizon-widget.js
ANYWIDGET_JS := $(OUT_DIR)/farmhorizon-anywidget.js
DEMO_JS      := $(OUT_DIR)/demo.js
DEMO_HTML    := $(OUT_DIR)/index.html

all: $(TARGET) $(WIDGET_JS) $(ANYWIDGET_JS) $(DEMO_JS) $(DEMO_HTML)

$(WIDGET_JS): widget/farmhorizon-widget.ts
	@mkdir -p $(dir $@)
	$(BUN) build widget/farmhorizon-widget.ts --target browser --outfile $@

$(ANYWIDGET_JS): widget/anywidget-entry.ts widget/farmhorizon-widget.ts $(SINGLE_JS)
	@mkdir -p $(dir $@)
	$(BUN) build widget/anywidget-entry.ts --target browser --outfile $@

$(DEMO_JS): $(DEMO_TS) widget/farmhorizon-widget.ts
	@mkdir -p $(dir $@)
	$(BUN) build demo/demo.ts --target browser --outfile $@

$(DEMO_HTML): demo/index.html
	@mkdir -p $(dir $@)
	cp $< $@

$(MAIN_OBJ): main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB) $(LDFLAGS) -o $@
	@echo ""
	@echo "Build complete: $(TARGET)"

$(SINGLE_JS): $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(MAIN_OBJ) $(LIB) $(RAYLIB_LIB) $(LDFLAGS) -s SINGLE_FILE=1 -o $@

-include $(OBJ_DIR)/main.d

$(RAYLIB_LIB):
	@echo "Building Raylib for WebAssembly..."
	@test -d "$(RAYLIB_SRC)" || { echo "Error: raylib-src not found. Run 'make setup-wasm' from project root."; exit 1; }
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_WEB -j$$(nproc)

clean:
	rm -f $(OUT_DIR)/farmhorizon.js $(OUT_DIR)/farmhorizon.wasm $(SINGLE_JS)
	rm -f $(WIDGET_JS) $(ANYWIDGET_JS) $(DEMO_JS) $(DEMO_HTML)
	rm -f $(OUT_DIR)/index.js $(OUT_DIR)/index.wasm $(OUT_DIR)/index.data
	rm -rf $(OBJ_DIR)

clean-all: clean
	$(MAKE) -C $(RAYLIB_SRC) clean 2>/dev/null || true

.PHONY: all clean clean-all
