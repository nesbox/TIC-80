DUB_FLAGS = --quiet --build release --compiler ldc2 --arch wasm32-unknown-unknown-wasm
ifneq ($(origin WASI_SDK_PATH), undefined)
	override DUB_FLAGS += --config wasi
endif

build:
	dub build ${DUB_FLAGS}

clean:
	rm -rf cart.wasm .dub
