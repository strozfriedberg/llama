#include "readseek_c_api.h"
#include "readseek.h"

#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

int64_t readseek_read(void* obj, size_t len, uint8_t* buf) {
    try {
        return ((ReadSeek*)obj)->read(len, buf);
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "readseek_read: %s\n", e.what());
        return -1;
    }
}

size_t readseek_tellg(void* obj) {
    return ((ReadSeek*)obj)->tellg();
}

size_t readseek_seek(void* obj, size_t pos) {
    return ((ReadSeek*)obj)->seek(pos);
}

size_t readseek_size(void* obj) {
    return ((ReadSeek*)obj)->size();
}


#ifdef __cplusplus
}
#endif
