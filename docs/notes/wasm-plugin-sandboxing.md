# WASM Plugin Sandboxing

Keep the existing C ABI for first-party native plugins. Add WASM as a sandboxed tier for community plugins.

## Runtime Choice: WAMR

[WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) (WebAssembly Micro Runtime):

- Pure C — compiles as part of the Meson build, no external shared library
- Interpreter mode (dev) + AOT compilation (production, 1.2-1.8x native)
- Bytecode Alliance maintained (same org as Wasmtime)
- Automatic guest-pointer-to-native-pointer translation for host callbacks

```c
static int32_t host_read_bytes(wasm_exec_env_t exec_env, uint8_t *buf, int32_t len) {
    // buf is already a native pointer — WAMR auto-converted it
    MyContext *ctx = wasm_runtime_get_user_data(exec_env);
    return ctx->read(buf, len);
}

static NativeSymbol native_symbols[] = {
    {"read_bytes", (void*)host_read_bytes, "(*~)i", NULL}
};
```

## Alternatives Considered

| Runtime | Pros | Cons |
|---------|------|------|
| [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) | Pure C, embeds in build, interpreter+AOT | Less ecosystem than Wasmtime |
| [Extism](https://extism.org/) ([C++ SDK](https://github.com/extism/cpp-sdk)) | Highest-level API, handles serialization | Ships a Rust-built libextism.so dependency |
| [wasm3](https://github.com/wasm3/wasm3) | Tiny (~64KB), trivial to embed | Interpreter-only (5-15x slower), maintenance slowed |
| [Wasmtime](https://github.com/bytecodealliance/wasmtime) (C API) | Best JIT performance | Requires linking prebuilt shared library |

## What WASM Gives Us

- Memory safety enforced by runtime (plugin can't corrupt host)
- Capability-based security (only access what's explicitly granted via host functions)
- Cross-platform binaries (one .wasm for Windows/macOS/Linux)
- Language-agnostic (Rust, C, Go, Zig, AssemblyScript all compile to WASM)

## Precedent

- [Envoy Proxy](https://github.com/proxy-wasm/spec) — C++, WASM for network filter extensions
- Microsoft Flight Simulator — WASM for third-party instruments
- [ScyllaDB](https://www.scylladb.com/) — Wasmtime for UDFs
- [Zed editor](https://zed.dev/) — Rust, WASM for extensions

## Architecture

Two tiers in PluginManager:
1. **Native** (C ABI) — first-party, audited, maximum performance
2. **WASM** (WAMR) — community, sandboxed, cross-platform

Same dispatch interface, different loading paths. Plugin manifest declares which tier.
