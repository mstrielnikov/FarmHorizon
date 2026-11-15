# Master Makefile for FarmHorizon Recursive Build System
# ──────────────────────────────────────────────────
# Targets:
#   make gui        Build native GUI (C++ + Raylib)          → bin/FarmHorizon-gui
#   make wasm       Build the WebAssembly widget + demo page    → build-wasm/ (farmhorizon.js, index.html)
#   make run        Run native GUI
#   make test       Run headless unit tests (core + geometry, no window needed)
#   make serve      Build & serve the widget demo via Bun (http://localhost:8080)
#   make clean      Remove build artifacts
#   make setup-wasm Bootstrap EMSDK + Python sandbox
#
# Both targets link the same platform-agnostic library built from src/
# (build/native/libfarmhorizon.a and build/web/libfarmhorizon.a).

all: gui

gui:
	$(MAKE) -C apps/native

wasm:
	$(MAKE) -C apps/web

# ── Run ──────────────────────────────────────────

run: gui
	@./bin/FarmHorizon-gui

test:
	@$(MAKE) --no-print-directory -C tests

serve: wasm
	@echo "Serving at http://localhost:8080"
	@bun run tools/serve.ts

# ── Wasm Toolchain Bootstrap ─────────────────────

setup-wasm:
	@echo "=== Setting up EMSDK sandbox ==="
	@test -d emsdk || git clone https://github.com/emscripten-core/emsdk.git emsdk
	@cd emsdk && ./emsdk install latest && ./emsdk activate latest
	@echo "=== Fetching standalone Python 3.11 ==="
	@test -d python || ( \
		curl -sL https://github.com/indygreg/python-build-standalone/releases/download/20240415/cpython-3.11.9+20240415-x86_64-unknown-linux-gnu-install_only.tar.gz | tar xz && \
		ln -sf python3.11 python/bin/python3 \
	)
	@echo "=== Fetching Raylib source ==="
	@test -d raylib-src || git clone --depth 1 https://github.com/raysan5/raylib.git raylib-src
	@echo "Done. Run 'make wasm' to build."

# ── Clean ────────────────────────────────────────

clean:
	$(MAKE) -C apps/native clean
	$(MAKE) -C apps/web clean 2>/dev/null || true
	$(MAKE) -C tests clean
	rm -rf bin build

clean-all: clean
	$(MAKE) -C apps/web clean-all 2>/dev/null || true

.PHONY: all gui wasm run test serve setup-wasm clean clean-all
