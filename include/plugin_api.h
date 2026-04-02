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
    int64_t   (*read)(void* opaque, uint8_t* buf, size_t len);
    int64_t   (*seek)(void* opaque, size_t pos);
    uint64_t  (*size)(void* opaque);
} LlamaReadSeek;

typedef struct {
    const char*   file_signature;   /* NULL if no signature matched */
    uint64_t      inode_addr;
    uint64_t      file_size;
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    const char* name;
    const char* version;
} LlamaPluginInfo;

/* Plugin lifecycle */
LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle);
LLAMA_PLUGIN_EXPORT void            llama_plugin_shutdown(void);

/* Per-file processing — returns 0 on success/skip, negative on error */
LLAMA_PLUGIN_EXPORT int             llama_plugin_process(const LlamaFileContext* ctx);

/* Error reporting — returns thread-local string describing last error */
LLAMA_PLUGIN_EXPORT const char*     llama_plugin_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* LLAMA_PLUGIN_API_H */
