#include "processor.h"

#include <hasher/api.h>

#include <lightgrep/api.h>

#include "blocksequence.h"
#include "filerecord.h"
#include "hex.h"
#include "outputhandler.h"
#include "readseek.h"
#include "timer.h"

//#include <iostream>

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

Processor::Processor(LlamaDB* db, const std::shared_ptr<ProgramHandle>& prog):
  Db(db),
  DbConn(*db),
  LgProg(prog),
  Ctx(prog.get() ? lg_create_context(prog.get(), &ctxOpts) : nullptr, lg_destroy_context),
  Hasher(sfhash_create_hasher(SFHASH_MD5 | SFHASH_SHA_1 | SFHASH_SHA_2_256 | SFHASH_BLAKE3 | SFHASH_FUZZY), sfhash_destroy_hasher),
  Hashes(std::make_unique<HashBatch>()),
  ProcTimeTotal(0)
{
}

std::shared_ptr<Processor> Processor::clone() const {
  return std::make_shared<Processor>(Db, LgProg);
}

void Processor::process(ReadSeek& stream) {
  // std::cerr << "hashing..." << std::endl;
  // hash 'em if ya got 'em
  bool hashSuccess = false;
  {
    Timer procTime;
    SFHASH_HashValues hashes;
    hashSuccess = hashFile(Hasher.get(), stream, Buf, hashes);
    ProcTimeTotal += procTime.elapsed();
  }
/*  if (hashSuccess) {
    rec.Doc["md5"] = hexEncode(rec.Hashes.Md5, rec.Hashes.Md5 + sizeof(rec.Hashes.Md5));
    rec.Doc["sha1"] = hexEncode(rec.Hashes.Sha1, rec.Hashes.Sha1 + sizeof(rec.Hashes.Sha1));
    rec.Doc["sha256"] = hexEncode(rec.Hashes.Sha2_256, rec.Hashes.Sha2_256 + sizeof(rec.Hashes.Sha2_256));
    rec.Doc["blake3"] = hexEncode(rec.Hashes.Blake3, rec.Hashes.Blake3 + sizeof(rec.Hashes.Blake3));
    rec.Doc["fuzzy"] = hexEncode(rec.Hashes.Fuzzy, rec.Hashes.Fuzzy + sizeof(rec.Hashes.Fuzzy));
    rec.Doc["entropy"] = rec.Hashes.Entropy;
  }*/
//  out.outputInode(rec);
}

