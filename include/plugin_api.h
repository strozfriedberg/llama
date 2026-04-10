#ifndef LLAMA_PLUGIN_API_H
#define LLAMA_PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

#include "arrow/c/abi.h"

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
    const char*   sha256;          /* hex-encoded SHA-256 hash, always set */
    const char*   path;            /* full path including filename, or NULL */
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    uint32_t    struct_size;       /* sizeof(LlamaPluginInfo) */
    const char* name;
    const char* version;
} LlamaPluginInfo;

/* Table declaration: plugin-owned table name and Arrow schema. */
typedef struct {
    const char*        table_name;
    struct ArrowSchema schema;
} LlamaTableDef;

/* Callback to write one Arrow record batch to a declared table.
   table_name:  which table to write to (must match a declared table).
   schema:      Arrow schema for this batch.
   array:       Arrow array (record batch) to append.
   Returns 0 on success, negative on failure. */
typedef int (*LlamaWriteArrow)(
    void*                opaque,
    const char*          table_name,
    struct ArrowSchema*  schema,
    struct ArrowArray*   array
);

/* Write context passed to llama_plugin_process. */
typedef struct {
    void*           opaque;
    LlamaWriteArrow write;
} LlamaWriteContext;

/* Plugin lifecycle.
   Fills in info with plugin metadata.
   Sets *tables to an array of LlamaTableDef (plugin-owned, valid until shutdown).
   Sets *num_tables to the number of tables declared.
   Returns 0 on success, negative on failure. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_init(LlamaPluginInfo* info,
                                            const LlamaTableDef** tables,
                                            size_t* num_tables);
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void);

/* Per-file processing -- returns 0 on success/skip, negative on error.
   On error, *errmsg is set to a plugin-allocated error string.
   Caller must pass *errmsg to llama_plugin_free_error() when done.
   Called from multiple threads concurrently -- plugins must be thread-safe. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_process(const LlamaFileContext* ctx,
                                               const LlamaWriteContext* write_ctx,
                                               const char** errmsg);

/* Free an error string returned via llama_plugin_process. */
LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg);

#ifdef __cplusplus
}
#endif

#endif /* LLAMA_PLUGIN_API_H */
