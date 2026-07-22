.PHONY: build test verify-discovery bench asan clean

RELEASE_DIR := .build/release
ASAN_DIR := .build/asan

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

bench: build
	./$(RELEASE_DIR)/cfr_bench

SLOW_UNDER_SANITIZERS := leduc cfr\+ (deterministic|exploitability|checkpoint round-trip)

asan:
	cmake -B $(ASAN_DIR) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
	cmake --build $(ASAN_DIR)
	ctest --test-dir $(ASAN_DIR) --output-on-failure -E "$(SLOW_UNDER_SANITIZERS)"

clean:
	rm -rf .build .deps
