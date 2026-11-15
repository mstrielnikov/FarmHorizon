# FarmHorizon — Multi-Language Workspace Makefile
# ──────────────────────────────────────────────────────────────────────────────
# Layout:
#   data/       Input CSV datasets (data/inputs/) & pipeline outputs (data/outputs/)
#   engine/     C++20 isometric field renderer (native Raylib + Emscripten Wasm)
#   web/        TypeScript widget SDK, anywidget bundle, demo & Bun dev server
#   python/     marimo dashboard app, farmhorizon package, notebooks & tests
#   .toolchain/ Local bootstrapped SDKs (emsdk, standalone python, raylib, vcpkg)
#   build/      All compiled/bundled build artifacts
#
# Targets:
#   make gui          Build native desktop GUI           → build/bin/FarmHorizon-gui
#   make wasm         Build Wasm engine + TS/anywidget   → build/wasm/
#   make run          Build & run native desktop GUI
#   make test         Run C++ headless tests + marimo static check
#   make serve        Build & serve standalone web demo  (http://localhost:8080)
#   make marimo       Build & serve marimo dashboard     (http://localhost:8080)
#   make marimo-edit  Open marimo dashboard in edit mode (http://localhost:8080)
#   make setup-py     Create python/.venv & install editable farmhorizon package
#   make setup-wasm   Bootstrap .toolchain/{emsdk,python,raylib-src}
#   make clean        Remove build artifacts

.DEFAULT_GOAL := gui

PY_VENV      := python/.venv
PY_BOOTSTRAP := $(shell test -x .toolchain/python/bin/python3 && echo .toolchain/python/bin/python3 || echo python3)

# ── Build ────────────────────────────────────────

gui:
	$(MAKE) -C engine native

wasm:
	$(MAKE) -C engine wasm
	$(MAKE) -C web

# ── Run & Serve ──────────────────────────────────

run: gui
	@./build/bin/FarmHorizon-gui

serve: wasm
	@$(MAKE) --no-print-directory -C web serve

marimo: wasm
	@test -x $(PY_VENV)/bin/marimo || $(MAKE) setup-py
	@echo "Serving marimo app at http://localhost:8080"
	@$(PY_VENV)/bin/marimo run python/app.py --port 8080 --headless

marimo-edit: wasm
	@test -x $(PY_VENV)/bin/marimo || $(MAKE) setup-py
	@echo "Opening marimo editor at http://localhost:8080"
	@$(PY_VENV)/bin/marimo edit python/app.py --port 8080 --headless

# ── Test ─────────────────────────────────────────

test:
	@$(MAKE) --no-print-directory -C engine test
	@if [ -x $(PY_VENV)/bin/marimo ]; then $(PY_VENV)/bin/marimo check python/app.py; fi

# ── Environment Bootstrap ────────────────────────

setup-py:
	@test -x $(PY_VENV)/bin/python || $(PY_BOOTSTRAP) -m venv $(PY_VENV)
	@$(PY_VENV)/bin/pip install -e ./python

setup-wasm:
	@mkdir -p .toolchain
	@echo "=== Setting up EMSDK sandbox (.toolchain/emsdk) ==="
	@test -d .toolchain/emsdk || git clone https://github.com/emscripten-core/emsdk.git .toolchain/emsdk
	@cd .toolchain/emsdk && ./emsdk install latest && ./emsdk activate latest
	@echo "=== Fetching standalone Python 3.11 (.toolchain/python) ==="
	@test -d .toolchain/python || ( \
		mkdir -p .toolchain && \
		curl -sL https://github.com/indygreg/python-build-standalone/releases/download/20240415/cpython-3.11.9+20240415-x86_64-unknown-linux-gnu-install_only.tar.gz | tar xz -C .toolchain && \
		ln -sf python3.11 .toolchain/python/bin/python3 \
	)
	@echo "=== Fetching Raylib source (.toolchain/raylib-src) ==="
	@test -d .toolchain/raylib-src || git clone --depth 1 https://github.com/raysan5/raylib.git .toolchain/raylib-src
	@echo "Done. Run 'make wasm' to build."

# ── Clean ────────────────────────────────────────

clean:
	$(MAKE) -C engine clean
	$(MAKE) -C web clean
	rm -rf build

clean-all: clean
	$(MAKE) -C engine clean-all

.PHONY: gui wasm run serve marimo marimo-edit test setup-py setup-wasm clean clean-all
