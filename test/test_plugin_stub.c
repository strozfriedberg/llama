#include "plugin_api.h"
#include <string.h>
#include <stdlib.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(LlamaPluginInfo* info,
                                           const LlamaTableDef** tables,
                                           size_t* num_tables) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "test-plugin";
    info->version = "0.1.0";
    *tables = NULL;
    *num_tables = 0;
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx,
                                              const LlamaWriteContext* write_ctx,
                                              const char** errmsg) {
    (void)write_ctx;
    if (ctx->file_signature && strcmp(ctx->file_signature, "SQLite Database") == 0) {
        /* Read first 16 bytes to verify ReadSeek works */
        uint8_t buf[16];
        int64_t n = ctx->readseek.read(ctx->readseek.opaque, buf, 16);
        if (n < 0) {
            *errmsg = strdup("read failed");
            return -1;
        }
        return 0;
    }
    return 0;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
