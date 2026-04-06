#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info) {
    (void)duckdb_handle;
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "error-plugin";
    info->version = "0.1.0";
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx, const char** errmsg) {
    (void)ctx;
    *errmsg = strdup("deliberate test error");
    return -1;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
