# ABOUTME: Dockerfile for building and running llama on Linux arm64
# ABOUTME: Designed to mount ~/code for access to source repositories

FROM ubuntu:22.04

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

# Install meson (newer version than apt provides)
RUN pip3 install meson

# Create working directory for builds
WORKDIR /build

# Set installation prefix
ENV PREFIX=/usr/local
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}
ENV LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH}

# Copy build script
COPY docker-build-deps.sh /build/build-all.sh
RUN chmod +x /build/build-all.sh

# Default command
CMD ["/bin/bash"]
