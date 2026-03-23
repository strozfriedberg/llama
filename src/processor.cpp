#include "processor.h"
#include "progressinfo.h"
#include "tskconversion.h"

#include "timestamps.h"
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
                                   const MagicsType& sigMagics,
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
  ExceptionAppender(DbConn.get(), "exception_log"),
  LgCtx(Context->Prog.get() ? lg_create_context(Context->Prog.get(), &ctxOpts) : nullptr, lg_destroy_context),
  Hasher(sfhash_create_hasher(Context->getSupportedHashAlgsFromContext()), sfhash_destroy_hasher),
  HashRecord(),
  Hashes(std::make_unique<HashBatch>()),
  SearchHits(std::make_unique<DBBatch<SearchHit>>()),
  FileSigs(std::make_unique<FileSigBatch>()),
  Exceptions(std::make_unique<ExceptionBatch>()),
  SigAnalyzer(Context->SigProg, Context->SigMagics),
  ProcTimeTotal(0),
  Img(nullptr, tsk_img_close)
{
  Buf.reserve(1 << 20);
}

std::shared_ptr<Processor> Processor::clone() const {
  return std::make_shared<Processor>(Context);
}

void Processor::createReadSeek(Entry& entry) {
  if (!Img) {
    const char* path = entry.EvidenceFile.c_str();
    Img.reset(tsk_img_open_utf8(1, &path, TSK_IMG_TYPE_DETECT, 0));
    if (!Img) {
      throw std::runtime_error("Failed to open image: " + entry.EvidenceFile);
    }
  }
  auto [itr, absent] = FsHandles.try_emplace(entry.FsOffset, nullptr);
  if (absent) {
    itr->second = std::shared_ptr<TSK_FS_INFO>(
      tsk_fs_open_img(Img.get(), entry.FsOffset, entry.FsType),
      tsk_fs_close
    );
    if (!itr->second) {
      FsHandles.erase(itr);
      throw std::runtime_error("Failed to open filesystem at offset " + std::to_string(entry.FsOffset));
    }
  }
  entry.setStream(std::make_unique<ReadSeekTSK>(itr->second, entry.Addr));
}

void Processor::process(Entry& entry) {
  SFHASH_HashValues h;
  {
    Timer procTime;
    try {
      hashFile(Hasher.get(), entry.getStream(), Buf, h);
    } catch (const EvidenceIOError& e) {
      logException(entry, "hash", e.what());
      ProcTimeTotal += procTime.elapsed();
      return;
    }
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
  bool hasPdfSig = false;
  {
    try {
      std::vector<MagicPtr> sigResults;
      entry.getStream().seek(0);
      SigAnalyzer.getSignatures(entry.getStream(), sigResults);
      for (const auto& sig : sigResults) {
        FileSigs->add(FileSigResult{HashRecord.Blake3, sig->Id});
        if (sig->Id == "8343f9e2-601f-4e88-8a78-09a3b5f906eb") {
          hasPdfSig = true;
        }
      }
    } catch (const EvidenceIOError& e) {
      logException(entry, "signature", e.what());
    }
  }

  {
    Timer procTime;
    try {
      if (hasPdfSig) {
        PDFReader reader;
        reader.readTextFromPDF(entry.getStream());
        char* text = reader.getExtractedText();
        if (text) {
          ReadSeekBuf rs(text);
          search(rs);
        }
        else {
          search(entry.getStream());
        }
      }
      else {
        search(entry.getStream());
      }
    } catch (const EvidenceIOError& e) {
      logException(entry, "search", e.what());
    }
    ProcTimeTotal += procTime.elapsed();
  }
}

void Processor::processBatch(const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries) {
  uint64_t pendingInodes = 0;
  uint64_t pendingBytes = 0;
  for (auto& entry : *entries) {
    if (entry->getStream().open()) {
      pendingBytes += entry->getStream().size();
      process(*entry);
      entry->getStream().close();
      ++pendingInodes;
      if (pendingBytes >= 128 * 1024) {
        if (Context->Progress) {
          Context->Progress->update(pendingInodes, pendingBytes);
        }
        pendingInodes = 0;
        pendingBytes = 0;
      }
    }
  }
  flush();
  if (Context->Progress && (pendingInodes > 0 || pendingBytes > 0)) {
    Context->Progress->update(pendingInodes, pendingBytes);
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
  if (Exceptions->size()) {
    Exceptions->copyToDB(ExceptionAppender.get());
    ExceptionAppender.flush();
    Exceptions->clear();
  }
}

void Processor::logException(const Entry& entry, const char* operation, const char* message) {
  ExceptionRecord rec;
  rec.EvidenceFile = entry.EvidenceFile;
  rec.FsIndex = entry.FsIndex;
  rec.FsOffset = entry.FsOffset;
  rec.Addr = entry.Addr;
  rec.AddrFlags = TskUtils::metaFlags(entry.AddrFlags);
  rec.Path = entry.Path;
  rec.FileSize = entry.FileSize;
  rec.Operation = operation;
  rec.Timestamp = nowISO();
  rec.Message = message;

  Exceptions->add(rec);

  if (Context->Progress) {
    Context->Progress->addException();
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
