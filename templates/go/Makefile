# Build dependencies
GO = tinygo
WASM_OPT = wasm-opt

# Whether to build for debugging instead of release
DEBUG = 0

# Compilation flags
GOFLAGS = -target ./target.json -panic trap
ifeq ($(DEBUG), 1)
	GOFLAGS += -opt 1
else
	GOFLAGS += -opt z -no-debug
endif

# wasm-opt flags
WASM_OPT_FLAGS = -Oz --zero-filled-memory --strip-producers --enable-bulk-memory

all:
	@mkdir -p build
	$(GO) build $(GOFLAGS) -o build/cart.wasm .
ifneq ($(DEBUG), 1)
ifeq (, $(shell command -v $(WASM_OPT)))
	@echo Tip: $(WASM_OPT) was not found. Install it from binaryen for smaller builds!
else
	$(WASM_OPT) $(WASM_OPT_FLAGS) build/cart.wasm -o build/cart.wasm
endif
endif

# Build the pix example (see examples/pix)
pix:
	@mkdir -p build
	$(GO) build $(GOFLAGS) -o build/pix.wasm ./examples/pix
ifneq ($(DEBUG), 1)
ifneq (, $(shell command -v $(WASM_OPT)))
	$(WASM_OPT) $(WASM_OPT_FLAGS) build/pix.wasm -o build/pix.wasm
endif
endif

# Build the bdr example (see examples/bdr)
bdr:
	@mkdir -p build
	$(GO) build $(GOFLAGS) -o build/bdr.wasm ./examples/bdr
ifneq ($(DEBUG), 1)
ifneq (, $(shell command -v $(WASM_OPT)))
	$(WASM_OPT) $(WASM_OPT_FLAGS) build/bdr.wasm -o build/bdr.wasm
endif
endif

# Build the bdr2 example (see examples/bdr2)
bdr2:
	@mkdir -p build
	$(GO) build $(GOFLAGS) -o build/bdr2.wasm ./examples/bdr2
ifneq ($(DEBUG), 1)
ifneq (, $(shell command -v $(WASM_OPT)))
	$(WASM_OPT) $(WASM_OPT_FLAGS) build/bdr2.wasm -o build/bdr2.wasm
endif
endif

# Build the ttri example (see examples/ttri)
ttri:
	@mkdir -p build
	$(GO) build $(GOFLAGS) -o build/ttri.wasm ./examples/ttri
ifneq ($(DEBUG), 1)
ifneq (, $(shell command -v $(WASM_OPT)))
	$(WASM_OPT) $(WASM_OPT_FLAGS) build/ttri.wasm -o build/ttri.wasm
endif
endif

# Build the ttri2 sprite rotation example (see examples/ttri2)
ttri2:
	@mkdir -p build
	$(GO) build $(GOFLAGS) -o build/ttri2.wasm ./examples/ttri2
ifneq ($(DEBUG), 1)
ifneq (, $(shell command -v $(WASM_OPT)))
	$(WASM_OPT) $(WASM_OPT_FLAGS) build/ttri2.wasm -o build/ttri2.wasm
endif
endif

.PHONY: clean
clean:
	rm -rf build
