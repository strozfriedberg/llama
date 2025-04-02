#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t readseek_read(void* obj, size_t len, uint8_t* buf);

size_t readseek_tellg(void* obj);

size_t readseek_seek(void* obj, size_t pos);

size_t readseek_size(void* obj);

#ifdef __cplusplus
}
#endif
