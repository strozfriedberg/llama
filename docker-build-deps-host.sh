#!/bin/bash
# ABOUTME: Host wrapper script that runs dependency builds inside Docker container
# ABOUTME: Creates ~/build-linux directory structure and mounts it for persistent builds

set -e

# Parse arguments
CLEAN_FLAG=""
if [ "$1" = "--clean" ]; then
    CLEAN_FLAG="--clean"
fi

# Ensure build directory exists on host
mkdir -p ~/build-linux

# Run the container with mounted directories
# Note: /code/llama/docker-entrypoint.sh sets all environment variables
docker run --platform linux/arm64 --rm \
    -v ~/code:/code \
    -v ~/build-linux:/build-linux \
    llama:latest \
    /build/docker-entrypoint.sh /build/docker-build-deps.sh $CLEAN_FLAG
