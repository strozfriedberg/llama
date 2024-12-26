#pragma once

#include "llamaduck.h"
#include "duckhash.h"
#include "llamabatch.h"
#include <lightgrep/search_hit.h>

#include <memory>
#include <vector>

struct SFHASH_Hasher;

struct ProgramHandle;
struct ContextHandle;

struct FileRecord;
class OutputHandler;
class ReadSeek;

class Processor {
public:
  Processor(LlamaDB* db, const std::shared_ptr<ProgramHandle>& prog, const std::vector<std::string>& patternToRuleId);

  std::shared_ptr<Processor> clone() const;

  void process(ReadSeek& stream);

  void flush(void);

  Processor(const Processor&) = delete;

  double getProcessorTime() const { return ProcTimeTotal; }

  void search(ReadSeek& rs);

  void addToSearchHitBatch(const LG_SearchHit* const hit);

  void currentHash(const std::string& hash) { CurrentHash = hash; }

  DBBatch<SearchHit>* searchHits() { return SearchHits.get(); }

private:
  const std::vector<std::string>& PatternToRuleId;

  std::vector<unsigned char> Buf; // to avoid reallocations

  std::string CurrentHash; // for use with search hit - blake3 hash of file currently being processed

  LlamaDB* const Db; // weak pointer, allows for clone()
  LlamaDBConnection DbConn;
  LlamaDBAppender   Appender;

  std::shared_ptr<ProgramHandle> LgProg; // shared
  std::shared_ptr<ContextHandle> Ctx; // not shared, could be unique_ptr
  std::shared_ptr<SFHASH_Hasher> Hasher; // not shared, could be unique_ptr
  std::unique_ptr<HashBatch> Hashes;
  std::unique_ptr<DBBatch<SearchHit>> SearchHits;

  double ProcTimeTotal;
};

