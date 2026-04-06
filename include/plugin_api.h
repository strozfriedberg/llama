#ifndef LLAMA_PLUGIN_API_H
#define LLAMA_PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
  #define LLAMA_PLUGIN_EXPORT __declspec(dllexport)
#else
  #define LLAMA_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void*     opaque;
    /* Note: parameter order is (buf, len), matching Rust's Read::read convention.
       C++ ReadSeek::read uses (len, buf) -- the bridge handles the transposition. */
    int64_t   (*read)(void* opaque, uint8_t* buf, size_t len);
    int64_t   (*seek)(void* opaque, size_t pos);   /* returns new position, or negative on error */
    uint64_t  (*size)(void* opaque);
} LlamaReadSeek;

typedef struct {
    uint32_t      struct_size;     /* sizeof(LlamaFileContext) */
    const char*   file_signature;  /* NULL if no signature matched */
    uint64_t      inode_addr;
    uint64_t      file_size;
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    uint32_t    struct_size;       /* sizeof(LlamaPluginInfo) */
    const char* name;
    const char* version;
} LlamaPluginInfo;

/* Plugin lifecycle.
   duckdb_handle is a duckdb_connection for DB access during init only.
   Fills in info with plugin metadata. Returns 0 on success, negative on failure. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info);
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void);

/* Per-file processing -- returns 0 on success/skip, negative on error.
   On error, *errmsg is set to a plugin-allocated error string.
   Caller must pass *errmsg to llama_plugin_free_error() when done.
   Called from multiple threads concurrently -- plugins must be thread-safe. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_process(const LlamaFileContext* ctx, const char** errmsg);

/* Free an error string returned via llama_plugin_process. */
LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg);

#ifdef __cplusplus
}
#endif

#endif /* LLAMA_PLUGIN_API_H */
