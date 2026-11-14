# Shared build settings for all FarmHorizon platform targets.
# Platform Makefiles set WORKSPACE_ROOT before including this file.

CXXSTD   := -std=c++20
WARNINGS := -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter
INCLUDES := -I$(WORKSPACE_ROOT)/include

# Directories
BIN_DIR    := $(WORKSPACE_ROOT)/bin
BUILD_ROOT := $(WORKSPACE_ROOT)/build
