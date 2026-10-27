.PHONY: build test test-gcc verify-discovery asan tsan clean

RELEASE_DIR := .build/release
ASAN_DIR := .build/asan
TSAN_DIR := .build/tsan
GCC_IMAGE := cfr-gcc
GCC_VOLUME := cfr-gcc-build
GSI_JAR_URL := https://www.cs.cmu.edu/~gilpin/GSI.jar
GSI_JAR_SHA256 := 24ab45f190cf7c54ed9bf2cbe4227e7e97ff886bed31b0cbd41df562b2737eba

build:
	cmake -B $(RELEASE_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_DIR)

data/gsi/partition:
	@tmp_jar=$$(mktemp /tmp/gsi.XXXXXX.jar); \
	tmp_dir=$$(mktemp -d /tmp/gsi.XXXXXX); \
	trap 'rm -f "$$tmp_jar"; rm -rf "$$tmp_dir"' EXIT INT TERM; \
	echo "downloading GSI data from $(GSI_JAR_URL)"; \
	if ! curl -fsSL -o "$$tmp_jar" "$(GSI_JAR_URL)"; then \
		echo "error: failed to download GSI.jar from $(GSI_JAR_URL)"; \
		exit 1; \
	fi; \
	if command -v shasum >/dev/null 2>&1; then \
		actual_sha=$$(shasum -a 256 "$$tmp_jar" | awk '{print $$1}'); \
	elif command -v sha256sum >/dev/null 2>&1; then \
		actual_sha=$$(sha256sum "$$tmp_jar" | awk '{print $$1}'); \
	else \
		echo "error: need shasum or sha256sum to verify GSI.jar"; \
		exit 1; \
	fi; \
	if [ "$$actual_sha" != "$(GSI_JAR_SHA256)" ]; then \
		echo "error: checksum mismatch for GSI.jar"; \
		echo "expected $(GSI_JAR_SHA256) but got $$actual_sha"; \
		exit 1; \
	fi; \
	unzip -o -j "$$tmp_jar" 'strategy/*' -d "$$tmp_dir" >/dev/null; \
	for file in partition player1 player2; do \
		if [ ! -f "$$tmp_dir/$$file" ]; then \
			echo "error: GSI.jar missing strategy/$$file"; \
			exit 1; \
		fi; \
	done; \
	mkdir -p data/gsi; \
	cp "$$tmp_dir/partition" data/gsi/partition; \
	cp "$$tmp_dir/player1" data/gsi/player1; \
	cp "$$tmp_dir/player2" data/gsi/player2

verify-discovery: build
	@declared=$$(grep -rhc '^TEST_CASE(' tests/*.cpp | paste -sd+ - | bc); \
	discovered=$$(ctest --test-dir $(RELEASE_DIR) -N | tail -1 | awk '{print $$3}'); \
	if [ "$$declared" != "$$discovered" ]; then \
		echo "test discovery mismatch: $$declared TEST_CASE macros, $$discovered ctest entries"; \
		echo "a '[' or ']' in a test name silently merges every later test into one entry"; \
		exit 1; \
	fi; \
	echo "test discovery verified: $$declared test cases"

test: data/gsi/partition verify-discovery
	ctest --test-dir $(RELEASE_DIR) --output-on-failure

test-gcc: data/gsi/partition
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
