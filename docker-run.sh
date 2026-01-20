#!/bin/bash
# ABOUTME: Host wrapper for fast llama iteration - builds, tests, and optionally runs shell
# ABOUTME: Uses persistent build artifacts from ~/build-linux for ~30 second rebuilds

set -e

# Ensure build directory exists
if [ ! -d ~/build-linux/lib ]; then
    echo "ERROR: Dependencies not built yet. Run ./docker-build-deps-host.sh first."
    exit 1
fi

# Detect if we're in an interactive terminal
TTY_FLAG=""
if [ -t 0 ] && [ -t 1 ]; then
    TTY_FLAG="-it"
fi

# Common docker options
DOCKER_OPTS=(
    --platform linux/arm64
    --rm
    $TTY_FLAG
    -v ~/code:/code
    -v ~/build-linux:/build-linux
    -v "${HOME}/ev:/ev:ro"
    -v "${HOME}/tmp:/host-tmp"
    --privileged
    --device /dev/fuse
    -w /code/llama
)

if [ $# -eq 0 ]; then
    # Default: build + test + shell
    docker run "${DOCKER_OPTS[@]}" llama:latest /build/docker-entrypoint.sh bash -c '
        set -e
        echo "=== Setting up llama build ==="
        meson setup /build-linux/build/llama --prefix=/build-linux 2>/dev/null || true

        echo ""
        echo "=== Building llama ==="
        meson compile -C /build-linux/build/llama

        echo ""
        echo "=== Running tests ==="
        meson test -C /build-linux/build/llama || echo "Some tests failed"

        echo ""
        echo "=== Build complete. Starting shell ==="
        exec /bin/bash
    '
else
    # Run specified command via entrypoint (so env vars are set)
    docker run "${DOCKER_OPTS[@]}" llama:latest /build/docker-entrypoint.sh "$@"
fi
