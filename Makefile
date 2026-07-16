.PHONY: build test bench asan

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
