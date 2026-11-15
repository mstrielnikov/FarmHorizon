# Native (host) toolchain detection, shared by engine/targets/native and engine/tests.
# Requires: TOOLCHAIN. Provides: CXX, AR, TOOLCHAIN_FLAGS, TOOLCHAIN_LDFLAGS.

# Prefer a bundled .toolchain compiler when present; fall back to host clang++/c++.
ifneq ($(wildcard $(TOOLCHAIN)/llvm/bin/clang++),)
  CXX               := $(TOOLCHAIN)/llvm/bin/clang++
  AR                := $(TOOLCHAIN)/llvm/bin/llvm-ar
  TOOLCHAIN_FLAGS   := -stdlib=libc++
  TOOLCHAIN_LDFLAGS := -fuse-ld=lld -stdlib=libc++ -Wl,-rpath,'$$ORIGIN/../../.toolchain/llvm/lib/x86_64-unknown-linux-gnu'
else ifneq ($(wildcard $(TOOLCHAIN)/emsdk/upstream/bin/clang++),)
  CXX               := $(TOOLCHAIN)/emsdk/upstream/bin/clang++
  AR                := $(TOOLCHAIN)/emsdk/upstream/bin/llvm-ar
  TOOLCHAIN_FLAGS   := $(if $(wildcard /usr/include),--gcc-toolchain=/usr,)
  TOOLCHAIN_LDFLAGS :=
else
  CXX               ?= c++
  AR                ?= ar
  TOOLCHAIN_FLAGS   :=
  TOOLCHAIN_LDFLAGS :=
endif
