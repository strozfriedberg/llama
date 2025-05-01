#pragma once

#include "llamaduck.h"
#include "duckhash.h"
#include "llamabatch.h"
#include "pdfreader.h"
#include <lightgrep/search_hit.h>

#include <memory>
#include <vector>

#include "boost/interprocess/file_mapping.hpp"
#include "boost/interprocess/mapped_region.hpp"

namespace bip = boost::interprocess;

struct SFHASH_Hasher;
struct SFHASH_Hashset;

struct ProgramHandle;
struct ContextHandle;

struct FileRecord;
class OutputHandler;
class ReadSeek;



struct HashsetBundle {
  HashsetBundle(const char* path);
  ~HashsetBundle();

  bip::file_mapping HashsetMapping;
  bip::mapped_region HashsetRegion;
  SFHASH_Hashset* Hashset;

};

struct ProcessorContext {
  ProcessorContext(
    LlamaDB* db,
    const std::shared_ptr<ProgramHandle>& prog,
    const std::vector<std::string>& patternToRuleId,
    const std::string& exclusionHsetPath,
    const std::string& inclusionHsetPath
  );

  LlamaDB* Db;
  const std::shared_ptr<ProgramHandle> Prog;
  const std::vector<std::string>& PatternToRuleId;
  std::unique_ptr<HashsetBundle> ExclusionHashset;
  std::unique_ptr<HashsetBundle> InclusionHashset;
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

