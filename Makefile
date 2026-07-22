.PHONY: build test bench asan test-fallback clean

RELEASE_DIR := .build/release
ASAN_DIR := .build/asan
FALLBACK_DIR := .build/fallback

build:
	cmake -B $(RELEASE_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR)

test: build
	ctest --test-dir $(RELEASE_DIR) --output-on-failure

bench: build
	./$(RELEASE_DIR)/cfr_bench

asan:
	cmake -B $(ASAN_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
	cmake --build $(ASAN_DIR)
	ctest --test-dir $(ASAN_DIR) --output-on-failure

test-fallback:
	cmake -B $(FALLBACK_DIR) -DCMAKE_BUILD_TYPE=Release -DCFR_DISABLE_AVX2=ON
	cmake --build $(FALLBACK_DIR)
	ctest --test-dir $(FALLBACK_DIR) --output-on-failure

clean:
	rm -rf .build .deps
