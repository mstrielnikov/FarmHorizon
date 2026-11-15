# FarmHorizon C++20 Engine Makefile
# Orchestrates native desktop GUI, Emscripten Wasm modules, and headless C++ tests.

.DEFAULT_GOAL := native

native:
	$(MAKE) -C targets/native

wasm:
	$(MAKE) -C targets/wasm

test:
	@$(MAKE) --no-print-directory -C tests

clean:
	$(MAKE) -C targets/native clean
	$(MAKE) -C targets/wasm clean 2>/dev/null || true
	$(MAKE) -C tests clean

clean-all: clean
	$(MAKE) -C targets/wasm clean-all 2>/dev/null || true

.PHONY: native wasm test clean clean-all
