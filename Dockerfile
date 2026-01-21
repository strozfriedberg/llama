# ABOUTME: Dockerfile for building and running llama on Linux arm64
# ABOUTME: Designed to mount ~/code for access to source repositories

FROM ubuntu:24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies and system libraries
RUN apt-get update && apt-get install -y \
    # Build tools
    build-essential \
    autoconf \
    automake \
    libtool \
    pkg-config \
    git \
    cmake \
    bison \
    flex \
    # C++ dependencies
    libboost-all-dev \
    libicu-dev \
    libarchive-dev \
    libfuzzy-dev \
    libyara-dev \
    # Additional tools
    ninja-build \
    python3 \
    python3-pip \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Filesystem userspace tools for mounting evidence
RUN apt-get update && apt-get install -y \
    # FUSE support
    fuse3 \
    libfuse3-dev \
    # Filesystem tools
    xfsprogs \
    btrfs-progs \
    f2fs-tools \
    exfatprogs \
    ntfs-3g \
    # Loop device support
    mount \
    util-linux \
    && rm -rf /var/lib/apt/lists/*

# Install Rust for pdf_extractor
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install cargo-c for pdf_extractor
RUN cargo install cargo-c

# Install meson via pipx (Ubuntu 24.04 PEP 668 compliant)
RUN apt-get update && apt-get install -y pipx && rm -rf /var/lib/apt/lists/*
RUN pipx install meson
ENV PATH="/root/.local/bin:${PATH}"

# Install Catch2 v3 for unit tests
RUN git clone --depth 1 --branch v3.5.3 https://github.com/catchorg/Catch2.git /tmp/Catch2 && \
    cd /tmp/Catch2 && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build build --target install && \
    rm -rf /tmp/Catch2

# Create working directory for builds
WORKDIR /build

# Set installation prefix
ENV PREFIX=/usr/local
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}
ENV LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH}

# Copy build scripts
COPY docker-entrypoint.sh /build/docker-entrypoint.sh
COPY docker-build-deps.sh /build/docker-build-deps.sh
RUN chmod +x /build/docker-entrypoint.sh /build/docker-build-deps.sh && \
    ln -s /build/docker-build-deps.sh /build/build-all.sh

# Default command
CMD ["/bin/bash"]
