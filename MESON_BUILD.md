# Meson Build System for Llama

This document describes the alternate Meson build system for Llama, which complements the existing autotools build.

## Prerequisites

Install Meson build system:
```bash
# On macOS with Homebrew
brew install meson

# On Ubuntu/Debian
sudo apt install meson

# On Fedora/RHEL
sudo dnf install meson

# Via pip
pip install meson
```

## Basic Usage

### Setup build directory
```bash
meson setup builddir
```

### Build the project
```bash
meson compile -C builddir
```

### Run tests (if Catch2 is available)
```bash
meson test -C builddir
```

### Install
```bash
meson install -C builddir
```

## Build Options

### Debug build
```bash
meson setup builddir --buildtype=debug
```

### Release build
```bash
meson setup builddir --buildtype=release
```

### Enable Address Sanitizer
```bash
meson setup builddir -Dasan=true
```

## Dependencies

The Meson build system requires the same dependencies as the autotools build:

**Required:**
- Boost (date_time, program_options, system)
- Lightgrep
- The Sleuth Kit (libtsk)
- DuckDB
- libarchive
- libhasher
- jsoncons (header-only)

**Optional:**
- YARA (for rule-based analysis)
- Catch2 (for testing)

## Files Structure

- `meson.build` - Root build file with project configuration and dependencies
- `meson_options.txt` - Build options (e.g., ASAN support)
- `src/meson.build` - Main executable build configuration
- `test/meson.build` - Test suite build configuration

## Comparison with Autotools

Both build systems are maintained and should produce equivalent results:

| Feature | Autotools | Meson |
|---------|-----------|-------|
| Configuration | `./configure` | `meson setup builddir` |
| Build | `make -j8` | `meson compile -C builddir` |
| Test | `make -j8 check` | `meson test -C builddir` |
| Install | `make install` | `meson install -C builddir` |
| Clean | `make clean` | `rm -rf builddir` |

## Cross-compilation

Meson has excellent cross-compilation support. Create a cross-file and use:
```bash
meson setup builddir --cross-file=cross_file.txt
```