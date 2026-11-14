# Headless unit tests for the raylib-free layers (core + geometry).
# No window, GPU or raylib needed:  make test   (from the project root)

WORKSPACE_ROOT := ..
include $(WORKSPACE_ROOT)/common.mk
include $(WORKSPACE_ROOT)/native-toolchain.mk

.DEFAULT_GOAL := run

# Only layers that must stay renderer-agnostic are linked here. If one of them
# starts including raylib, this build breaks — which is the point.
TESTED_SRCS := $(wildcard $(WORKSPACE_ROOT)/src/geometry/*.cpp)
TEST_SRCS   := $(wildcard *.cpp)

TEST_BIN := $(BUILD_ROOT)/tests/farmhorizon-tests
CXXFLAGS := $(CXXSTD) $(WARNINGS) -g $(TOOLCHAIN_FLAGS) $(INCLUDES)

$(TEST_BIN): $(TEST_SRCS) $(TESTED_SRCS) $(wildcard *.hpp) $(shell find $(WORKSPACE_ROOT)/include -name '*.hpp')
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(TESTED_SRCS) $(TOOLCHAIN_LDFLAGS) -o $@

run: $(TEST_BIN)
	@$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)

.PHONY: run clean
