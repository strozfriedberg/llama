#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(LlamaPluginInfo* info,
                                           const LlamaTableDef** tables,
                                           size_t* num_tables) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "error-plugin";
    info->version = "0.1.0";
    *tables = NULL;
    *num_tables = 0;
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx,
                                              const LlamaWriteContext* write_ctx,
                                              const char** errmsg) {
    (void)ctx;
    (void)write_ctx;
    *errmsg = strdup("deliberate test error");
    return -1;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
