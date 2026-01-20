#!/bin/bash
# ABOUTME: Builds all llama dependencies using out-of-tree builds in /build-linux
# ABOUTME: Designed to run inside Docker via docker-entrypoint.sh which sets env vars

set -e

# Verify environment is set up (docker-entrypoint.sh should have done this)
if [ -z "$BUILD_LINUX" ] || [ -z "$PREFIX" ]; then
    echo "ERROR: Environment not configured. Run via docker-entrypoint.sh"
    exit 1
fi

# Handle --clean flag
if [ "$1" = "--clean" ]; then
    echo "=== Cleaning all build artifacts ==="
    rm -rf "${BUILD_LINUX:?}"/*
fi

# Create directory structure
mkdir -p "$BUILD_LINUX"/{build,lib,bin,include}
mkdir -p "$BUILD_LINUX/lib/pkgconfig"
mkdir -p "$BUILD_LINUX/build"/{sleuthkit,lightgrep,hasher,rust,llama}

echo "=== Building dependencies from /code ==="
echo "PREFIX=$PREFIX"
echo "PKG_CONFIG_PATH=$PKG_CONFIG_PATH"

# Build Sleuth Kit (libtsk)
if [ -d /code/sleuthkit ]; then
    echo ""
    echo "=== Building sleuthkit ==="
    cd /code/sleuthkit
    if [ ! -f configure ]; then
        ./bootstrap
    fi
    cd "$BUILD_LINUX/build/sleuthkit"
    /code/sleuthkit/configure --prefix="$PREFIX"
    make -j$(nproc)
    make install
else
    echo "ERROR: /code/sleuthkit not found"
    exit 1
fi

# Build lightgrep
if [ -d /code/lightgrep ]; then
    echo ""
    echo "=== Building lightgrep ==="
    cd /code/lightgrep
    if [ ! -f configure ]; then
        autoreconf -fi
    fi
    cd "$BUILD_LINUX/build/lightgrep"
    /code/lightgrep/configure --prefix="$PREFIX"
    make -j$(nproc)
    make install
else
    echo "ERROR: /code/lightgrep not found"
    exit 1
fi

# Build hasher
if [ -d /code/hasher ]; then
    echo ""
    echo "=== Building hasher ==="
    cd /code/hasher
    if [ ! -f configure ]; then
        autoreconf -fi
    fi
    cd "$BUILD_LINUX/build/hasher"
    /code/hasher/configure --prefix="$PREFIX"
    make -j$(nproc)
    make install
else
    echo "ERROR: /code/hasher not found"
    exit 1
fi

# Build pdf_extractor
if [ -d /code/pdf_extractor ]; then
    echo ""
    echo "=== Building pdf_extractor ==="
    cd /code/pdf_extractor
    cargo cinstall --release --prefix="$PREFIX" --libdir="$PREFIX/lib"
else
    echo "ERROR: /code/pdf_extractor not found"
    exit 1
fi

# Check for DuckDB - try to install from system or build from source
if ! pkg-config --exists duckdb; then
    echo ""
    echo "=== Installing DuckDB ==="
    # Try installing from system packages first
    apt-get update && apt-get install -y libduckdb-dev || {
        # If not available, build from source
        if [ -d /code/duckdb ]; then
            echo "Building DuckDB from source..."
            cd /code/duckdb
            make -j$(nproc)
            cd build/release
            cmake --install . --prefix "$PREFIX"
        else
            echo "WARNING: DuckDB not found in system or /code/duckdb"
            echo "Attempting to download and build..."
            cd /tmp
            if [ ! -d duckdb ]; then
                git clone https://github.com/duckdb/duckdb.git --depth 1
            fi
            cd duckdb
            make -j$(nproc)
            cd build/release
            cmake --install . --prefix "$PREFIX"
        fi
    }
fi

# Install jsoncons headers if not present
if [ ! -d "$PREFIX/include/jsoncons" ]; then
    echo ""
    echo "=== Installing jsoncons ==="
    cd /tmp
    if [ ! -d jsoncons ]; then
        git clone https://github.com/danielaparker/jsoncons.git --depth 1
    fi
    cd jsoncons
    cp -r include/jsoncons "$PREFIX/include/"
fi

# Build e01 library
if [ -d /code/e01 ]; then
    echo ""
    echo "=== Building e01 ==="
    cd /code/e01
    if cargo build --release; then
        echo "e01 library built successfully"
        # Try to build e01mount, but don't fail if it doesn't work
        if [ -d /code/e01/fuse ]; then
            echo "Building e01mount..."
            if (cd /code/e01/fuse && cargo build --release); then
                echo "e01mount built at: $CARGO_TARGET_DIR/release/e01mount"
            else
                echo "WARNING: e01mount build failed, continuing without it"
            fi
        fi
    else
        echo "WARNING: e01 build failed, continuing without it"
    fi
else
    echo "WARNING: /code/e01 not found, skipping e01 build"
fi

echo ""
echo "=== Dependency build complete ==="
echo "Libraries installed to: $PREFIX/lib"
echo "Headers installed to: $PREFIX/include"
echo "Binaries installed to: $PREFIX/bin"
