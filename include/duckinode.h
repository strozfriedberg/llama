#pragma once

#include "inode.h"
#include "llamaduck.h"

struct DuckInode : public SchemaType<Inode,
                                      const char*,
                                      const char*,
                                      const char*,
                                      uint64_t,
                                      uint64_t,
                                      uint64_t,
                                      uint64_t,
                                      uint64_t,
                                      const char*,
                                      uint64_t,
                                      uint64_t,
                                      const char*,
                                      const char*,
                                      const char*,
                                      const char*>
{};

class InodeBatch : public DuckBatch {
public:
  void add(const Inode& inode);

  unsigned int copyToDB(duckdb_appender& appender);
};
