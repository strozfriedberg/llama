#include "fieldhasher.h"

#include <cstring>
#include <ostream>

std::ostream& operator<<(std::ostream& o, const FieldHash& h) {
  constexpr size_t kHashBytes = std::tuple_size<decltype(FieldHash::hash)>::value;
  char hstr[kHashBytes * 2 + 1];
  sfhash_hex(&hstr[0], h.hash.data(), kHashBytes);
  hstr[kHashBytes * 2] = '\0';
  return o << hstr;
}

void FieldHasher::reset() {
  sfhash_reset_hasher(Hasher.get());
}

FieldHash FieldHasher::get_hash() {
  sfhash_get_hashes(Hasher.get(), &Hashes);
  FieldHash h;
  std::memcpy(h.hash.data(), &Hashes.Sha2_256, h.hash.size());
  return h;
}

void FieldHasher::hash_it(const char* s) {
  hash_it_null_terminated(s, s + std::strlen(s));
}

void FieldHasher::hash_it(const std::string& s) {
  hash_it_null_terminated(s.data(), s.data() + s.length());
}

void FieldHasher::hash_it(const std::string_view& s) {
  hash_it_null_terminated(s.data(), s.data() + s.length());
}

void FieldHasher::hash_it_null_terminated(const void* beg, const void* end) {
  hash_it(beg, end);
  hash_it('\0');
}

void FieldHasher::hash_it(const void* beg, const void* end) {
  sfhash_update_hasher(Hasher.get(), beg, end);
}
