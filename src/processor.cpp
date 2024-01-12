#include "processor.h"

#include <hasher/api.h>

#include <lightgrep/api.h>

#include "blocksequence.h"
#include "filerecord.h"
#include "hex.h"
#include "outputhandler.h"

namespace {
  const LG_ContextOptions ctxOpts{0, 0};

  bool hashFile(SFHASH_Hasher* hasher, BlockSequence& content, SFHASH_HashValues& hashes) {
    auto i = content.begin();
    const auto end = content.end();
    if (i != end) {
      sfhash_reset_hasher(hasher);
      for ( ; i != end; ++i) {
        sfhash_update_hasher(hasher, i->first, i->second);
      }
      sfhash_get_hashes(hasher, &hashes);
      return true;
    }
    return false;
  }
}

Processor::Processor(const std::shared_ptr<ProgramHandle>& prog):
   LgProg(prog),
   Ctx(prog.get() ? lg_create_context(prog.get(), &ctxOpts): nullptr, lg_destroy_context),
   Hasher(sfhash_create_hasher(SFHASH_MD5 | SFHASH_SHA_1 | SFHASH_SHA_2_256 | SFHASH_BLAKE3 | SFHASH_FUZZY | SFHASH_ENTROPY), sfhash_destroy_hasher)
{
}

std::shared_ptr<Processor> Processor::clone() const {
  return std::make_shared<Processor>(LgProg);
}

void Processor::process(FileRecord& rec, OutputHandler& out) {
  // std::cerr << "hashing..." << std::endl;
  // hash 'em if ya got 'em

  if (hashFile(Hasher.get(), *rec.Blocks, rec.Hashes)) {
    rec.Doc["md5"] = hexEncode(rec.Hashes.Md5, rec.Hashes.Md5 + sizeof(rec.Hashes.Md5));
    rec.Doc["sha1"] = hexEncode(rec.Hashes.Sha1, rec.Hashes.Sha1 + sizeof(rec.Hashes.Sha1));
    rec.Doc["sha256"] = hexEncode(rec.Hashes.Sha2_256, rec.Hashes.Sha2_256 + sizeof(rec.Hashes.Sha2_256));
    rec.Doc["blake3"] = hexEncode(rec.Hashes.Blake3, rec.Hashes.Blake3 + sizeof(rec.Hashes.Blake3));
    rec.Doc["fuzzy"] = hexEncode(rec.Hashes.Fuzzy, rec.Hashes.Fuzzy + sizeof(rec.Hashes.Fuzzy));
    rec.Doc["entropy"] = rec.Hashes.Entropy;
  }
  out.outputInode(rec);
}