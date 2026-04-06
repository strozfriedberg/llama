#pragma once

#include "plugin_api.h"
#include "readseek.h"

inline LlamaReadSeek wrapReadSeek(ReadSeek* rs) {
  LlamaReadSeek lrs;
  lrs.opaque = static_cast<void*>(rs);

  // Note: C++ ReadSeek::read(len, buf) has reversed parameter order
  // vs C ABI read(opaque, buf, len). The C ABI order matches Rust's
  // Read::read(buf: &mut [u8]) convention.
  lrs.read = [](void* o, uint8_t* buf, size_t len) -> int64_t {
    try {
      return static_cast<int64_t>(static_cast<ReadSeek*>(o)->read(len, buf));
    } catch (...) {
      return -1;
    }
  };

  lrs.seek = [](void* o, size_t pos) -> int64_t {
    try {
      return static_cast<int64_t>(static_cast<ReadSeek*>(o)->seek(pos));
    } catch (...) {
      return -1;
    }
  };

  lrs.size = [](void* o) -> uint64_t {
    try {
      return static_cast<ReadSeek*>(o)->size();
    } catch (...) {
      return 0;
    }
  };

  return lrs;
}
