.PHONY: build test test-gcc verify-discovery asan tsan clean

RELEASE_DIR := .build/release
ASAN_DIR := .build/asan
TSAN_DIR := .build/tsan
GCC_IMAGE := cfr-gcc
GCC_VOLUME := cfr-gcc-build

build:
	cmake -B $(RELEASE_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR)

verify-discovery: build
	@declared=$$(grep -rhc '^TEST_CASE(' tests/*.cpp | paste -sd+ - | bc); \
	discovered=$$(ctest --test-dir $(RELEASE_DIR) -N | tail -1 | awk '{print $$3}'); \
	if [ "$$declared" != "$$discovered" ]; then \
		echo "test discovery mismatch: $$declared TEST_CASE macros, $$discovered ctest entries"; \
		echo "a '[' or ']' in a test name silently merges every later test into one entry"; \
		exit 1; \
	fi; \
	echo "test discovery verified: $$declared test cases"

test: verify-discovery
	ctest --test-dir $(RELEASE_DIR) --output-on-failure

test-gcc:
	docker build -t $(GCC_IMAGE) - < Dockerfile
	docker run --rm -v "$(CURDIR)":/src:ro \
		-v $(GCC_VOLUME):/build $(GCC_IMAGE) sh -c '\
		cmake -S /src -B /build/release \
			-DCMAKE_BUILD_TYPE=Release \
			-DFETCHCONTENT_BASE_DIR=/build/deps \
		&& cmake --build /build/release -j4 \
		&& ctest --test-dir /build/release --output-on-failure'

asan:
	cmake -B $(ASAN_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
	cmake --build $(ASAN_DIR)
	ctest --test-dir $(ASAN_DIR) --output-on-failure

tsan:
	cmake -B $(TSAN_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=thread" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
	cmake --build $(TSAN_DIR)
	ctest --test-dir $(TSAN_DIR) --output-on-failure

clean:
	rm -rf .build .deps
