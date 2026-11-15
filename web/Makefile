# FarmHorizon Web Workspace
# Bundles the TypeScript widget SDK, self-contained anywidget ESM, and demo page.

WORKSPACE_ROOT := ..
OUT_DIR        := $(WORKSPACE_ROOT)/build/wasm
BUN            ?= bun

SINGLE_JS    := $(OUT_DIR)/farmhorizon-single.js
WIDGET_JS    := $(OUT_DIR)/farmhorizon-widget.js
ANYWIDGET_JS := $(OUT_DIR)/farmhorizon-anywidget.js
DEMO_JS      := $(OUT_DIR)/demo.js
DEMO_HTML    := $(OUT_DIR)/index.html

DEMO_TS := $(wildcard demo/*.ts)

.DEFAULT_GOAL := all

all: $(WIDGET_JS) $(ANYWIDGET_JS) $(DEMO_JS) $(DEMO_HTML)

$(WIDGET_JS): widget/farmhorizon-widget.ts
	@mkdir -p $(dir $@)
	$(BUN) build widget/farmhorizon-widget.ts --target browser --outfile $@

$(ANYWIDGET_JS): widget/anywidget-entry.ts widget/farmhorizon-widget.ts $(SINGLE_JS)
	@mkdir -p $(dir $@)
	@test -f "$(SINGLE_JS)" || { echo "Error: $(SINGLE_JS) missing. Build engine wasm target first."; exit 1; }
	$(BUN) build widget/anywidget-entry.ts --target browser --outfile $@

$(DEMO_JS): $(DEMO_TS) widget/farmhorizon-widget.ts
	@mkdir -p $(dir $@)
	$(BUN) build demo/demo.ts --target browser --outfile $@

$(DEMO_HTML): demo/index.html
	@mkdir -p $(dir $@)
	cp $< $@

serve:
	@echo "Serving standalone demo at http://localhost:8080"
	@$(BUN) run tools/serve.ts

clean:
	rm -f $(WIDGET_JS) $(ANYWIDGET_JS) $(DEMO_JS) $(DEMO_HTML)

.PHONY: all serve clean
