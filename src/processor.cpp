#include "processor.h"

#include <hasher/api.h>

#include <lightgrep/api.h>

#include "blocksequence.h"
#include "filerecord.h"
#include "hex.h"
#include "outputhandler.h"
#include "readseek.h"
#include "timer.h"

namespace {
  const LG_ContextOptions ctxOpts{0, 0};

  bool hashFile(SFHASH_Hasher* hasher, ReadSeek& stream, std::vector<unsigned char>& buf, SFHASH_HashValues& hashes) {
    buf.reserve(1 << 20);
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
    return true;
  }
}

Processor::Processor(LlamaDB* db, const std::shared_ptr<ProgramHandle>& prog, const std::vector<std::string>& patternToRuleId):
  PatternToRuleId(patternToRuleId),
  Db(db),
  DbConn(*db),
  Appender(DbConn.get(), "hash"),
  LgProg(prog),
  Ctx(prog.get() ? lg_create_context(prog.get(), &ctxOpts) : nullptr, lg_destroy_context),
  Hasher(sfhash_create_hasher(SFHASH_MD5 | SFHASH_SHA_1 | SFHASH_SHA_2_256 | SFHASH_BLAKE3 | SFHASH_FUZZY), sfhash_destroy_hasher),
  Hashes(std::make_unique<HashBatch>()),
  SearchHits(std::make_unique<DBBatch<SearchHit>>()),
  ProcTimeTotal(0)
{
}

std::shared_ptr<Processor> Processor::clone() const {
  return std::make_shared<Processor>(Db, LgProg, PatternToRuleId);
}

void Processor::process(ReadSeek& stream) {
  // std::cerr << "hashing..." << std::endl;
  // hash 'em if ya got 'em
  bool hashSuccess = false;
  SFHASH_HashValues h;
  {
    Timer procTime;
    hashSuccess = hashFile(Hasher.get(), stream, Buf, h);
    ProcTimeTotal += procTime.elapsed();
  }
  if (hashSuccess) {
    HashRec hashes;
    hashes.MetaAddr = stream.getID();
    hashes.MD5 = hexEncode(h.Md5, h.Md5 + sizeof(h.Md5));
    hashes.SHA1 = hexEncode(h.Sha1, h.Sha1 + sizeof(h.Sha1));
    hashes.SHA256 = hexEncode(h.Sha2_256, h.Sha2_256 + sizeof(h.Sha2_256));
    hashes.Blake3 = hexEncode(h.Blake3, h.Blake3 + sizeof(h.Blake3));
    hashes.Ssdeep = hexEncode(h.Fuzzy, h.Fuzzy + sizeof(h.Fuzzy));

    currentHash(hashes.Blake3);

    Hashes->add(hashes);
  }
}

void Processor::flush(void) {
  if (Hashes->size()) {
    Hashes->copyToDB(Appender.get());
    SearchHits->copyToDB(Appender.get());
    Appender.flush();
  }
}

void handleSearchHit(void* userData, const LG_SearchHit* const hit) {
  reinterpret_cast<Processor*>(userData)->addToSearchHitBatch(hit);
}

void Processor::addToSearchHitBatch(const LG_SearchHit* const hit) {
  LG_PatternInfo* info = lg_prog_pattern_info(LgProg.get(), hit->KeywordIndex);
  std::string pat(info->Pattern);
  SearchHit sh = SearchHit{pat, hit->Start, hit->End, PatternToRuleId[hit->KeywordIndex], CurrentHash, pat.length()};
  SearchHits->add(sh);
}

void Processor::search(ReadSeek& rs) {
  lg_reset_context(Ctx.get());
  std::vector<uint8_t> buf;
  buf.reserve(1 << 20);
  size_t bytesRead = 0;
  uint64_t offset = 0;
  do {
      bytesRead = rs.read(1 << 20, buf);
      if (bytesRead > 0) {
        lg_search(Ctx.get(), (char*)buf.data(), (char*)buf.data() + bytesRead, offset, (void*)this, handleSearchHit);
      }
    } while (bytesRead > 0);
}
