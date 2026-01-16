#!/bin/bash
# ABOUTME: Script to build all llama dependencies from mounted /code directory
# ABOUTME: Designed to run inside the Docker container

set -e

echo "=== Building dependencies from /code ==="

# Build Sleuth Kit (libtsk)
if [ -d /code/sleuthkit ]; then
    echo "Building sleuthkit..."
    cd /code/sleuthkit
    if [ ! -f configure ]; then
        ./bootstrap
    fi
    ./configure --prefix=${PREFIX}
    make -j$(nproc)
    make install
    ldconfig
else
    echo "ERROR: /code/sleuthkit not found"
    exit 1
fi

# Build lightgrep
if [ -d /code/lightgrep ]; then
    echo "Building lightgrep..."
    cd /code/lightgrep
    if [ ! -f configure ]; then
        autoreconf -fi
    fi
    ./configure --prefix=${PREFIX}
    make -j$(nproc)
    make install
    ldconfig
else
    echo "ERROR: /code/lightgrep not found"
    exit 1
fi

# Build hasher
if [ -d /code/hasher ]; then
    echo "Building hasher..."
    cd /code/hasher
    if [ ! -f configure ]; then
        autoreconf -fi
    fi
    ./configure --prefix=${PREFIX}
    make -j$(nproc)
    make install
    ldconfig
else
    echo "ERROR: /code/hasher not found"
    exit 1
fi

# Build pdf_extractor
if [ -d /code/pdf_extractor ]; then
    echo "Building pdf_extractor..."
    cd /code/pdf_extractor
    cargo cinstall --release --prefix=${PREFIX} --libdir=${PREFIX}/lib
    ldconfig
else
    echo "ERROR: /code/pdf_extractor not found"
    exit 1
fi

# Check for DuckDB - try to install from system or build from source
if ! pkg-config --exists duckdb; then
    echo "Installing DuckDB..."
    # Try installing from system packages first
    apt-get update && apt-get install -y libduckdb-dev || {
        # If not available, build from source
        if [ -d /code/duckdb ]; then
            echo "Building DuckDB from source..."
            cd /code/duckdb
            make -j$(nproc)
            make install PREFIX=${PREFIX}
            ldconfig
        else
            echo "WARNING: DuckDB not found in system or /code/duckdb"
            echo "Attempting to download and build..."
            cd /tmp
            git clone https://github.com/duckdb/duckdb.git --depth 1
            cd duckdb
            make -j$(nproc)
            make install PREFIX=${PREFIX}
            ldconfig
        fi
    }
fi

# Install jsoncons headers if not present
if ! echo '#include <jsoncons/json.hpp>' | g++ -E - >/dev/null 2>&1; then
    echo "Installing jsoncons..."
    cd /tmp
    git clone https://github.com/danielaparker/jsoncons.git --depth 1
    cd jsoncons
    cp -r include/jsoncons ${PREFIX}/include/
fi

# Build e01 library and e01mount FUSE driver
if [ -d /code/e01 ]; then
    echo "Building e01..."
    cd /code/e01
    cargo build --release
    echo "Building e01mount..."
    cargo build --package e01mount --release
    echo "e01mount built at: /code/e01/target/release/e01mount"
else
    echo "WARNING: /code/e01 not found, skipping e01 build"
fi

# Build llama
if [ -d /code/llama ]; then
    echo "Building llama..."
    cd /code/llama
    meson setup builddir --prefix=${PREFIX}
    meson compile -C builddir
    echo "=== Build complete ==="
    echo "To run llama: ./builddir/src/llama"
else
    echo "ERROR: /code/llama not found"
    exit 1
fi
