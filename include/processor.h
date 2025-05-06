#pragma once

#include "llamaduck.h"
#include "duckhash.h"
#include "llamabatch.h"
#include "hashset.h"
#include "pdfreader.h"
#include "ruleengine.h"
#include <lightgrep/search_hit.h>

#include <memory>
#include <vector>

struct ProgramHandle;
struct ContextHandle;
struct FileRecord;
class OutputHandler;
class ReadSeek;


struct ProcessorContext {
  ProcessorContext(
    LlamaDB* db,
    const std::shared_ptr<ProgramHandle>& prog,
    const std::shared_ptr<LlamaRuleEngine> ruleEngine,
    const std::string& exclusionHsetPath,
    const std::string& inclusionHsetPath
  );

  // Returns hash flag to be used when initializing a SFHASH_Hasher, based
  // on what's supported by the context's hash sets.
  uint32_t getSupportedHashAlgsFromContext();

  LlamaDB* Db;
  const std::shared_ptr<ProgramHandle> Prog;
  const std::shared_ptr<LlamaRuleEngine> RuleEngine;
  std::unique_ptr<LlamaHashset> ExclusionHashset;
  std::unique_ptr<LlamaHashset> InclusionHashset;
};

class Processor {
public:
  Processor(std::shared_ptr<ProcessorContext> procContext);

  std::shared_ptr<Processor> clone() const;

  void process(ReadSeek& stream);

  void flush(void);

  Processor(const Processor&) = delete;

  double getProcessorTime() const { return ProcTimeTotal; }

  void search(ReadSeek& rs);

  void addToSearchHitBatch(const LG_SearchHit* const hit);

  // for testing purposes
  void setBlake3(const std::string& hash) { HashRecord.Blake3 = hash; }

  DBBatch<SearchHit>* searchHits() { return SearchHits.get(); }

  HashRec hashRecord() { return HashRecord; }

private:
  std::shared_ptr<ProcessorContext> Context;
  std::vector<unsigned char> Buf; // to avoid reallocations

  LlamaDBConnection DbConn;
  LlamaDBAppender   HashAppender;
  LlamaDBAppender   SearchHitAppender;

  std::shared_ptr<ContextHandle> LgCtx; // not shared, could be unique_ptr
  std::shared_ptr<SFHASH_Hasher> Hasher; // not shared, could be unique_ptr

  HashRec HashRecord; // to be reused per set of hashes

  std::unique_ptr<HashBatch> Hashes;
  std::unique_ptr<DBBatch<SearchHit>> SearchHits;

  double ProcTimeTotal;
};

