#!/bin/bash
# ABOUTME: Container entrypoint that sets up build environment variables
# ABOUTME: Single source of truth for all paths - used by all Docker workflows

# Build directory (mounted from host for persistence)
export BUILD_LINUX="${BUILD_LINUX:-/build-linux}"

# Installation prefix for all dependencies
export PREFIX="$BUILD_LINUX"

# Compiler/linker search paths
export PKG_CONFIG_PATH="$BUILD_LINUX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$BUILD_LINUX/lib:${LD_LIBRARY_PATH:-}"
export LIBRARY_PATH="$BUILD_LINUX/lib:${LIBRARY_PATH:-}"
export CPATH="$BUILD_LINUX/include:${CPATH:-}"

# Rust build directory (shared across all Rust projects)
export CARGO_TARGET_DIR="$BUILD_LINUX/build/rust"

# Execute the requested command
exec "$@"
