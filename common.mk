# Shared Compiler and Flags
# Use $(WORKSPACE_ROOT) to anchor paths to the project root
ifneq ($(wildcard $(WORKSPACE_ROOT)/.toolchain/llvm/bin/clang++),)
CXX      := $(WORKSPACE_ROOT)/.toolchain/llvm/bin/clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -g -I$(WORKSPACE_ROOT)/include -stdlib=libc++
LDFLAGS  := -fuse-ld=lld -static -stdlib=libc++ -lc++abi -lunwind -lpthread -ldl
else
CXX      := $(WORKSPACE_ROOT)/emsdk/upstream/bin/clang++
CXXFLAGS := -std=c++20 --gcc-toolchain=/usr -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -g -I$(WORKSPACE_ROOT)/include
LDFLAGS  := -lpthread -ldl
endif

# Directories
BIN_DIR := $(WORKSPACE_ROOT)/bin
BUILD_DIR := $(WORKSPACE_ROOT)/build
