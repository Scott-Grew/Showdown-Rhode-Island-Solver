.PHONY: build test bench asan test-fallback

build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

bench: build
	./build/cfr_bench

asan:
	cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
	cmake --build build-asan
	ctest --test-dir build-asan --output-on-failure

# Separate build dir, -mavx2 skipped and CFR_FORCE_SCALAR_BATCH defined so
# eval7.cpp's non-AVX2 branch actually gets compiled and exercised (V9),
# instead of only ever running on whatever machine happens to lack AVX2.
test-fallback:
	cmake -B build-fallback -DCMAKE_BUILD_TYPE=Release -DCFR_DISABLE_AVX2=ON
	cmake --build build-fallback
	ctest --test-dir build-fallback --output-on-failure
