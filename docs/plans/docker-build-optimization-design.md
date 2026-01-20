# Docker Build Optimization Design

## Problem Statement

Current workflow rebuilds all dependencies (~15-20 min) on every `docker run`, making fast iteration on llama impossible.

## Goals

- First-time build: 15-20 min (unavoidable)
- Subsequent llama-only builds: ~30 sec
- Transparent build artifacts on host filesystem
- Simple scripts with no complicated arguments
- Leverage native build systems for incremental compilation

## Design

### Workflow

1. **Build Docker image** (one time, or when Dockerfile changes):
   ```bash
   docker build --platform linux/arm64 -t llama:latest .
   ```

2. **Build dependencies** (one time, or when deps change):
   ```bash
   ./docker-build-deps.sh [--clean]
   ```

3. **Fast iteration on llama**:
   ```bash
   ./docker-run.sh              # build + test + shell
   ./docker-run.sh [command]    # run specific command
   ```

### Directory Structure

```
~/build-linux/
  build/              # out-of-tree build artifacts
    sleuthkit/        # autotools build dir
    lightgrep/        # autotools build dir
    hasher/           # autotools build dir
    rust/             # shared CARGO_TARGET_DIR
    llama/            # meson build dir
  lib/                # installed libraries (.so files)
  bin/                # installed executables
  include/            # installed headers
  lib/pkgconfig/      # pkg-config files
```

### Scripts

#### docker-build-deps.sh (host wrapper)

- Runs `docker run` with necessary mounts
- Passes through `--clean` flag to container script
- Invokes `/build/docker-build-deps.sh` inside container

#### /build/docker-build-deps.sh (container script)

**Behavior:**
1. If `--clean` flag: delete everything in `/build-linux/`
2. Set environment:
   ```bash
   export PREFIX=/build-linux
   export PKG_CONFIG_PATH=/build-linux/lib/pkgconfig:${PKG_CONFIG_PATH}
   export LD_LIBRARY_PATH=/build-linux/lib:${LD_LIBRARY_PATH}
   export CARGO_TARGET_DIR=/build-linux/build/rust
   ```
3. Build each dependency using out-of-tree builds:
   - **sleuthkit**: `mkdir -p /build-linux/build/sleuthkit && cd /build-linux/build/sleuthkit && /code/sleuthkit/configure --prefix=/build-linux && make -j$(nproc) && make install`
   - **lightgrep**: same pattern
   - **hasher**: same pattern
   - **pdf_extractor**: `cd /code/pdf_extractor && cargo cinstall --release --prefix=/build-linux --libdir=/build-linux/lib`
   - **e01**: `cd /code/e01 && cargo build --release`
   - **DuckDB, jsoncons**: existing logic
4. Run `ldconfig` after each library install
5. Defer to make/cargo for incremental compilation decisions

#### docker-run.sh (host wrapper)

- Runs `docker run` with necessary mounts
- Default behavior (no args): build llama + test + shell
- With args: run specified command

**Container behavior for default mode:**
1. `meson setup /build-linux/build/llama --prefix=/build-linux` (idempotent)
2. `meson compile -C /build-linux/build/llama`
3. `meson test -C /build-linux/build/llama`
4. `/bin/bash` (interactive shell)

**Container behavior with args:**
- Run the specified command directly

### Docker Mounts

Both scripts use the same mounts:
```bash
-v ~/code:/code                    # source code
-v ~/build-linux:/build-linux      # build artifacts
-v ~/ev:/ev:ro                     # evidence (read-only)
-v ~/tmp:/host-tmp                 # output location
--privileged                       # for FUSE
--device /dev/fuse                 # for mounting evidence
```

### Dockerfile Changes

- Copy `docker-build-deps.sh` to `/build/docker-build-deps.sh` (no renaming)
- Remove or update the old `/build/build-all.sh` references
- Ensure PREFIX and environment variables are set correctly

### Build System Integration

**Autotools (sleuthkit, lightgrep, hasher):**
- Out-of-tree builds: `mkdir build && cd build && ../configure && make`
- `make` handles incremental compilation via timestamps

**Cargo (pdf_extractor, e01):**
- `CARGO_TARGET_DIR=/build-linux/build/rust` consolidates artifacts
- Cargo handles incremental compilation automatically

**Meson (llama):**
- Requires out-of-tree builds by design
- `meson setup` is idempotent
- `meson compile` handles incremental compilation

### Expected Performance

| Operation | Time |
|-----------|------|
| First build (all deps) | 15-20 min |
| Rebuild llama only | ~30 sec |
| Rebuild one dep + llama | 2-5 min |
| Full clean rebuild | `./docker-build-deps.sh --clean` + 15-20 min |

## Implementation Tasks

1. Create `docker-build-deps.sh` (host wrapper)
2. Modify existing `docker-build-deps.sh` → `/build/docker-build-deps.sh` (container script)
3. Create `docker-run.sh` (host wrapper)
4. Update Dockerfile to copy scripts correctly
5. Update DOCKER.md with new workflow
6. Test the complete workflow

## Open Questions

None - design approved and ready for implementation.
