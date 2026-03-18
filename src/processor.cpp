#include "processor.h"
#include "progressinfo.h"

#include <hasher/api.h>

#include <lightgrep/api.h>

#include "blocksequence.h"
#include "entry.h"
#include "filerecord.h"
#include "outputhandler.h"
#include "readseek_impl.h"
#include "timer.h"
#include "util.h"
#include "pdfreader.h"
#include "ruleengine.h"

#include "boost/interprocess/file_mapping.hpp"
#include "boost/interprocess/mapped_region.hpp"

namespace bip = boost::interprocess;

namespace {
  const LG_ContextOptions ctxOpts{0, 0};

  void hashFile(SFHASH_Hasher* hasher, ReadSeek& stream, std::vector<unsigned char>& buf, SFHASH_HashValues& hashes) {
    stream.seek(0);
    sfhash_reset_hasher(hasher);
    size_t bytesRead = 0;
    do {
      bytesRead = stream.read(1 << 20, buf);
      if (bytesRead > 0) {
        sfhash_update_hasher(hasher, buf.data(), buf.data() + bytesRead);
      }
    } while (bytesRead > 0);
    sfhash_get_hashes(hasher, &hashes);
  }
}

ProcessorContext::ProcessorContext(LlamaDB* db,
                                   const std::shared_ptr<ProgramHandle>& prog,
                                   const std::shared_ptr<LlamaRuleEngine> ruleEngine,
                                   const std::string& exclusionHsetPath,
                                   const std::string& inclusionHsetPath,
                                   const FileSignatures::MagicsType& sigMagics,
                                   const std::shared_ptr<ProgramHandle>& sigProg,
                                   ProgressInfo* progress) :
  Db(db), Prog(prog), RuleEngine(ruleEngine), SigMagics(sigMagics), SigProg(sigProg), Progress(progress) {
  if (!exclusionHsetPath.empty()) {
    ExclusionHashset.reset(new LlamaHashset(exclusionHsetPath.c_str()));
  }

  if (!inclusionHsetPath.empty()) {
    InclusionHashset.reset(new LlamaHashset(inclusionHsetPath.c_str()));
    RuleEngine->addRuleRec(RuleRec{InclusionHashset->getHash(), InclusionHashset->getName()});
  }
}

uint32_t ProcessorContext::getSupportedHashAlgsFromContext() {
  // We always want to calculate the Blake3 hash because we use it for mapping files -> rule hits
  uint32_t hashAlgs = SFHASH_BLAKE3;

  if (ExclusionHashset) {
    hashAlgs |= ExclusionHashset->supportedHashAlg();
  }

  if (InclusionHashset) {
    hashAlgs |= InclusionHashset->supportedHashAlg();
  }

  return hashAlgs;
}

Processor::Processor(std::shared_ptr<ProcessorContext> procContext):
  Context(procContext),
  DbConn(*Context->Db),
  HashAppender(DbConn.get(), "hash"),
  SearchHitAppender(DbConn.get(), "search_hits"),
  RuleMatchAppender(DbConn.get(), "rule_hits"),
  FileSigAppender(DbConn.get(), "file_signatures"),
  LgCtx(Context->Prog.get() ? lg_create_context(Context->Prog.get(), &ctxOpts) : nullptr, lg_destroy_context),
  Hasher(sfhash_create_hasher(Context->getSupportedHashAlgsFromContext()), sfhash_destroy_hasher),
  HashRecord(),
  Hashes(std::make_unique<HashBatch>()),
  SearchHits(std::make_unique<DBBatch<SearchHit>>()),
  FileSigs(std::make_unique<FileSigBatch>()),
  SigAnalyzer(Context->SigProg ? FileSignatures::FileSigAnalyzer(Context->SigProg, Context->SigMagics)
                               : FileSignatures::FileSigAnalyzer(Context->SigMagics)),
  ProcTimeTotal(0)
{
  Buf.reserve(1 << 20);
}

std::shared_ptr<Processor> Processor::clone() const {
  return std::make_shared<Processor>(Context);
}

void Processor::process(Entry& entry) {
  SFHASH_HashValues h;
  {
    Timer procTime;
    hashFile(Hasher.get(), entry.getStream(), Buf, h);
    ProcTimeTotal += procTime.elapsed();
  }
  HashRecord.set(h, entry.Addr);

  if (Context->ExclusionHashset && Context->ExclusionHashset->lookup(h)) {
    // do something here if hash is in exclusion hset
  } else if (Context->InclusionHashset && Context->InclusionHashset->lookup(h)) {
    // add to rule hits here if hash in inclusion hset
    // since we want to treat inclusion hset as if it were another rule
  }

  // write hash record to database
  Hashes->add(HashRecord);

  // Detect file signatures
  {
    std::vector<FileSignatures::MagicPtr> sigResults;
    entry.getStream().seek(0);
    auto result = SigAnalyzer.getSignatures(entry.getStream(), sigResults);
    if (result.has_value()) {
      for (const auto& sig : sigResults) {
        FileSigs->add(FileSigResult{HashRecord.Blake3, sig->Id});
      }
    }
  }

  {
    Timer procTime;
    if (isPDF(entry.getStream())) {
      PDFReader reader;
      reader.readTextFromPDF(entry.getStream());
      ReadSeekBuf rs(reader.getExtractedText());
      // should this call process again? should we call process recursively for archives, for example?
      // what to do about the ReadSeek ID? ReadSeekBuf getID just returns 0...
      // what is the ID for the extracted text from a PDF? From a file within an archive?
      // for archive, is the ID of the file the same as the ID of the parent archive?
      // once process takes an Entry instead of a ReadSeek, we can add member attrs to 
      // the Entry class to differentiate different streams that comes from the same inode
      // maybe Entries can have their own unique IDs that are a function of the Addr, MetaAddr, and 
      // hash of the stream?
      // How do we handle duplicate archives in different locations?
      search(rs);
    }
    else {
      search(entry.getStream());
    }
    ProcTimeTotal += procTime.elapsed();
  }
}

void Processor::processBatch(const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries) {
  uint64_t batchInodes = 0;
  uint64_t batchBytes = 0;
  for (auto& entry : *entries) {
    if (entry->getStream().open()) {
      batchBytes += entry->getStream().size();
      process(*entry);
      entry->getStream().close();
      ++batchInodes;
    }
  }
  flush();
  if (Context->Progress) {
    Context->Progress->update(batchInodes, batchBytes);
  }
}

void Processor::flush(void) {
  if (Hashes->size()) {
    Hashes->copyToDB(HashAppender.get());
    SearchHits->copyToDB(SearchHitAppender.get());
    FileSigs->copyToDB(FileSigAppender.get());
    HashAppender.flush();
    SearchHitAppender.flush();
    FileSigAppender.flush();
    Hashes->clear();
    SearchHits->clear();
    FileSigs->clear();
  }
}

void handleSearchHit(void* userData, const LG_SearchHit* const hit) {
  reinterpret_cast<Processor*>(userData)->addToSearchHitBatch(hit);
}

void Processor::addToSearchHitBatch(const LG_SearchHit* const hit) {
  LG_PatternInfo* info = lg_prog_pattern_info(Context->Prog.get(), hit->KeywordIndex);
  std::string pat(info->Pattern);
  SearchHits->add(SearchHit{pat, hit->Start, hit->End, Context->RuleEngine->patternToRuleId()[hit->KeywordIndex], HashRecord.Blake3, hit->End - hit->Start});
}

void Processor::search(ReadSeek& rs) {
  if (!LgCtx) {
    return;
  }
  lg_reset_context(LgCtx.get());
  size_t bytesRead = 0;
  uint64_t offset = 0;
  rs.seek(0);
  do {
      bytesRead = rs.read(1 << 20, Buf);
      if (bytesRead > 0) {
        lg_search(LgCtx.get(), (char*)Buf.data(), (char*)Buf.data() + bytesRead, offset, (void*)this, handleSearchHit);
      }
      offset += bytesRead;
    } while (bytesRead > 0);

  lg_closeout_search(LgCtx.get(), (void*)this, handleSearchHit);
}
