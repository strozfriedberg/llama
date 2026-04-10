#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>

/* Static schema for a table with two columns: name (utf8), value (uint64) */
static struct ArrowSchema child_schemas[2];
static struct ArrowSchema* children[2] = { &child_schemas[0], &child_schemas[1] };
static struct ArrowSchema table_schema;
static LlamaTableDef table_defs[1];

LLAMA_PLUGIN_EXPORT int llama_plugin_init(LlamaPluginInfo* info,
                                          const LlamaTableDef** tables,
                                          size_t* num_tables) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "table-plugin";
    info->version = "0.1.0";

    /* Column 0: name (utf8 string) */
    memset(&child_schemas[0], 0, sizeof(struct ArrowSchema));
    child_schemas[0].format = "u";
    child_schemas[0].name = "name";
    child_schemas[0].n_children = 0;
    child_schemas[0].children = NULL;
    child_schemas[0].release = NULL;

    /* Column 1: value (uint64) */
    memset(&child_schemas[1], 0, sizeof(struct ArrowSchema));
    child_schemas[1].format = "L";
    child_schemas[1].name = "value";
    child_schemas[1].n_children = 0;
    child_schemas[1].children = NULL;
    child_schemas[1].release = NULL;

    /* Top-level struct schema */
    memset(&table_schema, 0, sizeof(struct ArrowSchema));
    table_schema.format = "+s";
    table_schema.name = "";
    table_schema.n_children = 2;
    table_schema.children = children;
    table_schema.release = NULL;

    table_defs[0].table_name = "plugin_test_data";
    table_defs[0].schema = table_schema;

    *tables = table_defs;
    *num_tables = 1;
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx,
                                             const LlamaWriteContext* write_ctx,
                                             const char** errmsg) {
    (void)ctx;
    (void)write_ctx;
    (void)errmsg;
    return 0; /* always skip */
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
