#!/bin/bash
# ABOUTME: Test script for end-to-end E01 → disk map visualization pipeline
# ABOUTME: Mounts E01, extracts extents with PosixReader, generates HTML visualization

set -e

echo "=== E01 to Disk Map Visualization Pipeline Test ==="
echo

echo "Step 1: Building llama in Docker..."
docker run --platform linux/arm64 -it --rm \
    -v ~/code:/code \
    llama:latest \
    bash -c "cd /code/llama && /build/build-all.sh"
echo "✓ Build complete"
echo

echo "Step 2: Starting Docker container with FUSE support..."
docker run --platform linux/arm64 -it --rm \
    --privileged \
    --device /dev/fuse \
    -v ~/code:/code \
    -v ~/ev:/ev:ro \
    llama:latest \
    bash -c '
set -e
echo "Step 3: Building e01mount..."
cd /code/e01
cargo build --package e01mount --release
echo "✓ e01mount built"
echo

echo "Step 4: Mounting E01 file..."
mkdir -p /tmp/e01mnt
/code/e01/target/release/e01mount /ev/starkskunk3.E01 /tmp/e01mnt -f &
E01_PID=$!
sleep 3
ls -la /tmp/e01mnt
echo "✓ E01 mounted"
echo

echo "Step 5: Mounting filesystem partition..."
mkdir -p /tmp/fs
# starkskunk3 has a single partition
mount -o ro,loop /tmp/e01mnt/disk-part1 /tmp/fs
ls -la /tmp/fs | head -20
echo "✓ Filesystem mounted"
echo

echo "Step 6: Creating output directory..."
rm -rf /tmp/output
mkdir -p /tmp/output
echo

echo "Step 7: Running llama search with PosixReader..."
cd /code/llama
./builddir/src/llama search --input posix:///tmp/fs --output /tmp/output
echo "✓ Llama search complete"
echo

echo "Step 8: Checking for disk map visualization..."
if [ -d "/tmp/output/diskmap" ]; then
    echo "✓ Disk map directory created"
    ls -la /tmp/output/diskmap
    if [ -f "/tmp/output/diskmap/index.html" ]; then
        echo "✓ index.html generated"
        wc -l /tmp/output/diskmap/index.html
    fi
    if ls /tmp/output/diskmap/chunk_*.json >/dev/null 2>&1; then
        echo "✓ Chunk files generated:"
        ls -lh /tmp/output/diskmap/chunk_*.json
    fi
else
    echo "✗ Disk map directory NOT created"
    exit 1
fi
echo

echo "Step 9: Checking database for extent and diskmap tables..."
# Use DuckDB CLI if available, or check parquet files
if [ -d "/tmp/output" ]; then
    echo "Output directory contents:"
    ls -la /tmp/output
    if [ -f "/tmp/output/extents.parquet" ]; then
        echo "✓ extents.parquet exists"
    fi
    if [ -f "/tmp/output/diskmap.parquet" ]; then
        echo "✓ diskmap.parquet exists"
    fi
fi
echo

echo "=== Pipeline Test Complete ==="
echo
echo "To view the visualization, copy /tmp/output/diskmap to your host and open index.html"

# Cleanup
umount /tmp/fs || true
kill $E01_PID 2>/dev/null || true
'

echo
echo "=== Test finished ==="
