#include "plugin_api.h"
#include <string.h>

static _Thread_local const char* last_err = NULL;

LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle) {
    (void)duckdb_handle;
    LlamaPluginInfo info;
    info.name = "test-plugin";
    info.version = "0.1.0";
    return info;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx) {
    if (ctx->file_signature && strcmp(ctx->file_signature, "SQLite Database") == 0) {
        /* Read first 16 bytes to verify ReadSeek works */
        uint8_t buf[16];
        int64_t n = ctx->readseek.read(ctx->readseek.opaque, buf, 16);
        if (n < 0) {
            last_err = "read failed";
            return -1;
        }
        return 0;
    }
    return 0;
}

LLAMA_PLUGIN_EXPORT const char* llama_plugin_last_error(void) {
    return last_err;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
    last_err = NULL;
}
