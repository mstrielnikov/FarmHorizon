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

LDFLAGS := -s USE_GLFW=3 \
           -s ASYNCIFY \
           -s TOTAL_MEMORY=512MB \
           --shell-file $(RAYLIB_SRC)/shell.html

# ── Output ───────────────────────────────────────

OUT_DIR := $(WORKSPACE_ROOT)/build-wasm
TARGET  := $(OUT_DIR)/index.html

all: $(TARGET)

$(TARGET): main.cpp $(LIB) $(RAYLIB_LIB)
	@mkdir -p $(dir $@) $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(OBJ_DIR)/main.d main.cpp $(LIB) $(RAYLIB_LIB) $(LDFLAGS) -o $@
	@echo ""
	@echo "Build complete: $(TARGET)"
	@echo "Run 'make serve' from project root."

-include $(OBJ_DIR)/main.d

$(RAYLIB_LIB):
	@echo "Building Raylib for WebAssembly..."
	@test -d "$(RAYLIB_SRC)" || { echo "Error: raylib-src not found. Run 'make setup-wasm' from project root."; exit 1; }
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_WEB -j$$(nproc)

clean:
	rm -f $(OUT_DIR)/index.html $(OUT_DIR)/index.js $(OUT_DIR)/index.wasm $(OUT_DIR)/index.data
	rm -rf $(OBJ_DIR)

clean-all: clean
	$(MAKE) -C $(RAYLIB_SRC) clean 2>/dev/null || true

.PHONY: all clean clean-all
