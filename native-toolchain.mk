# Native (host) toolchain detection, shared by apps/native and tests.
# Requires: WORKSPACE_ROOT. Provides: CXX, AR, TOOLCHAIN_FLAGS, TOOLCHAIN_LDFLAGS.

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
