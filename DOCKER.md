# Docker Build Instructions for llama (arm64)

This Docker setup enables fast iterative development of llama on Linux arm64 (Apple Silicon Macs).

## Prerequisites

- Docker Desktop for Mac with Apple Silicon support
- Source repositories cloned to `~/code/`:
  - `~/code/llama`
  - `~/code/sleuthkit`
  - `~/code/lightgrep`
  - `~/code/hasher`
  - `~/code/pdf_extractor`
  - `~/code/e01` (optional)

## Quick Start

```bash
# One-time: Build Docker image
docker build --platform linux/arm64 -t llama:latest .

# One-time: Build all dependencies (~15-20 min)
./docker-build-deps-host.sh

# Fast iteration (~30 sec)
./docker-run.sh
```

## Workflow

### 1. Build Docker Image (one time, or when Dockerfile changes)

```bash
docker build --platform linux/arm64 -t llama:latest .
```

### 2. Build Dependencies (one time, or when deps change)

```bash
./docker-build-deps-host.sh
```

This builds all dependencies and installs them to `~/build-linux/`. The build artifacts persist on your Mac's filesystem, so subsequent builds are incremental.

To force a clean rebuild of all dependencies:

```bash
./docker-build-deps-host.sh --clean
```

### 3. Fast Iteration on llama

```bash
./docker-run.sh
```

This:
1. Builds llama using meson (incremental, ~30 sec if deps unchanged)
2. Runs the test suite
3. Drops you into an interactive shell

To run a specific command instead:

```bash
./docker-run.sh meson compile -C /build-linux/build/llama
./docker-run.sh /build-linux/build/llama/src/llama --help
```

## Directory Structure

Build artifacts are stored on your Mac at `~/build-linux/`:

```
~/build-linux/
  build/              # Out-of-tree build artifacts
    sleuthkit/        # Autotools build dir
    lightgrep/        # Autotools build dir
    hasher/           # Autotools build dir
    rust/             # Shared CARGO_TARGET_DIR
    llama/            # Meson build dir
  lib/                # Installed libraries (.so files)
  bin/                # Installed executables
  include/            # Installed headers
  lib/pkgconfig/      # pkg-config files
```

## Environment Variables

All environment variables are set by `docker-entrypoint.sh` (single source of truth):

| Variable | Value | Purpose |
|----------|-------|---------|
| `BUILD_LINUX` | `/build-linux` | Root of persistent build directory |
| `PREFIX` | `/build-linux` | Installation prefix for dependencies |
| `PKG_CONFIG_PATH` | `/build-linux/lib/pkgconfig:...` | Find installed .pc files |
| `LD_LIBRARY_PATH` | `/build-linux/lib:...` | Runtime library search path |
| `LIBRARY_PATH` | `/build-linux/lib:...` | Compile-time library search path |
| `CPATH` | `/build-linux/include:...` | Header search path |
| `CARGO_TARGET_DIR` | `/build-linux/build/rust` | Shared Rust build artifacts |

## Docker Mounts

The scripts mount these directories:

| Host Path | Container Path | Purpose |
|-----------|---------------|---------|
| `~/code` | `/code` | Source repositories |
| `~/build-linux` | `/build-linux` | Build artifacts (persistent) |
| `~/ev` | `/ev` (read-only) | Evidence files |
| `~/tmp` | `/host-tmp` | Output location |

## Running llama on Evidence

The container runs with `--privileged` and FUSE support for mounting evidence:

```bash
./docker-run.sh bash

# Inside container:
/build-linux/build/llama/src/llama /ev/evidence.E01 /host-tmp/output
```

## Troubleshooting

**"Dependencies not built yet" error:**
Run `./docker-build-deps-host.sh` first to build all dependencies.

**Dependency build fails:**
Check that all required source repos exist in `~/code/`. For a clean rebuild, run `./docker-build-deps-host.sh --clean`.

**llama build fails with missing headers/libraries:**
Re-run `./docker-build-deps-host.sh` to rebuild and reinstall dependencies.

**Changes to sleuthkit/lightgrep not taking effect:**
The dependency build is incremental. If you've made source changes, just re-run `./docker-build-deps-host.sh` - make/autotools will only rebuild changed files.
