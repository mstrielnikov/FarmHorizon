# Shared build settings for all FarmHorizon C++ targets.
# Sub-Makefiles set WORKSPACE_ROOT and ENGINE_ROOT before including this file.

CXXSTD   := -std=c++20
WARNINGS := -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter
INCLUDES := -I$(ENGINE_ROOT)/include

# Unified workspace output directories
BUILD_ROOT := $(WORKSPACE_ROOT)/build
BIN_DIR    := $(BUILD_ROOT)/bin
WASM_DIR   := $(BUILD_ROOT)/wasm
TOOLCHAIN  := $(WORKSPACE_ROOT)/.toolchain
