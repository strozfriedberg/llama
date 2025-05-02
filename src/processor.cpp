#include "processor.h"

#include <hasher/api.h>

#include <lightgrep/api.h>

#include "blocksequence.h"
#include "filerecord.h"
#include "outputhandler.h"
#include "readseek_impl.h"
#include "timer.h"
#include "util.h"
#include "pdfreader.h"

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
                                   const std::vector<std::string>& patternToRuleId,
                                   const std::string& exclusionHsetPath,
                                   const std::string& inclusionHsetPath) : Db(db), Prog(prog), PatternToRuleId(patternToRuleId) {
  if (!exclusionHsetPath.empty()) {
    ExclusionHashset.reset(new LlamaHashset(exclusionHsetPath.c_str()));
  }

  if (!inclusionHsetPath.empty()) {
    InclusionHashset.reset(new LlamaHashset(inclusionHsetPath.c_str()));
  }
}

Processor::Processor(std::shared_ptr<ProcessorContext> procContext):
  Context(procContext),
  DbConn(*Context->Db),
  HashAppender(DbConn.get(), "hash"),
  SearchHitAppender(DbConn.get(), "search_hits"),
  LgCtx(Context->Prog.get() ? lg_create_context(Context->Prog.get(), &ctxOpts) : nullptr, lg_destroy_context),
  Hasher(sfhash_create_hasher(SFHASH_MD5 | SFHASH_SHA_1 | SFHASH_SHA_2_256 | SFHASH_BLAKE3 | SFHASH_FUZZY), sfhash_destroy_hasher),
  HashRecord(),
  Hashes(std::make_unique<HashBatch>()),
  SearchHits(std::make_unique<DBBatch<SearchHit>>()),
  ProcTimeTotal(0)
{
  Buf.reserve(1 << 20);
}

std::shared_ptr<Processor> Processor::clone() const {
  return std::make_shared<Processor>(Context);
}

void Processor::process(ReadSeek& stream) {
  SFHASH_HashValues h;
  {
    Timer procTime;
    hashFile(Hasher.get(), stream, Buf, h);
    ProcTimeTotal += procTime.elapsed();
  }
  HashRecord.set(h, stream.getID());

  // write hash record to database
  Hashes->add(HashRecord);

  {
    Timer procTime;
    if (isPDF(stream)) {
      PDFReader reader;
      reader.readTextFromPDF(stream);
      ReadSeekBuf rs(reader.getExtractedText());
      search(rs);
    }
    else {
      search(stream);
    }
    ProcTimeTotal += procTime.elapsed();
  }
}

void Processor::flush(void) {
  if (Hashes->size()) {
    Hashes->copyToDB(HashAppender.get());
    SearchHits->copyToDB(SearchHitAppender.get());
    HashAppender.flush();
    SearchHitAppender.flush();
  }
}

void handleSearchHit(void* userData, const LG_SearchHit* const hit) {
  reinterpret_cast<Processor*>(userData)->addToSearchHitBatch(hit);
}

void Processor::addToSearchHitBatch(const LG_SearchHit* const hit) {
  LG_PatternInfo* info = lg_prog_pattern_info(Context->Prog.get(), hit->KeywordIndex);
  std::string pat(info->Pattern);
  SearchHits->add(SearchHit{pat, hit->Start, hit->End, Context->PatternToRuleId[hit->KeywordIndex], HashRecord.Blake3, hit->End - hit->Start});
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
