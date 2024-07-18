#include "dirconversion.h"

#include "schema.h"

#include <system_error>
#include <fstream>

#include <boost/algorithm/string.hpp>
namespace fs = std::filesystem;

std::string DirUtils::fileTypeString(fs::file_type type) {
  switch (type) {
  case fs::file_type::none:
  case fs::file_type::not_found:
    return TYPE_UNDEF;
  case fs::file_type::regular:
    return TYPE_REG;
  case fs::file_type::directory:
    return TYPE_DIR;
  case fs::file_type::symlink:
    return TYPE_LNK;
  case fs::file_type::block:
    return TYPE_BLK;
  case fs::file_type::character:
    return TYPE_CHR;
  case fs::file_type::fifo:
    return TYPE_FIFO;
  case fs::file_type::socket:
    return TYPE_SOCK;
  case fs::file_type::unknown:
  default:
    return TYPE_UNDEF;
  }
}

fs::file_type DirUtils::fileType(const fs::directory_entry& de) {
  std::error_code err;
  const fs::file_status status = de.symlink_status(err);
  if (err) {
    std::cerr << "Error: " << de.path() << ": " << err.message() << std::endl;
    return fs::file_type::unknown;
  }

  return status.type();
}

std::uintmax_t DirUtils::nlink(const fs::directory_entry& de) {
  std::error_code err;
  const auto c = de.hard_link_count(err);
  if (err) {
    std::cerr << "Error: " << de.path() << ": " << err.message() << std::endl;
    return 0;
  }

  return c;
}

std::uintmax_t DirUtils::fileSize(const fs::directory_entry& de) {
  std::error_code err;
  const auto c = de.file_size(err);
  if (err) {
    if (err == std::errc::is_a_directory) {
      // ignore
    }
    else {
      std::cerr << "Error: " << de.path() << ": " << err.message() << std::endl;
    }
    return 0;
  }

  return c;
}

/*
fs::file_time_type DirUtils::mtime(const fs::directory_entry& de) {
  std::error_code err;
  const auto m = de.last_write_time(err);
  if (err) {
    std::cerr << "Error: " << err.message() << std::endl;
    return 0;
  }

  return m;
}
*/

jsoncons::json DirConverter::convertMeta(const fs::directory_entry& de) const {
  static const std::string flags = std::string(META_FLAG_ALLOC) + ", " + META_FLAG_USED;

  return jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "addr",        jsoncons::null_type() },
      { "flags",       flags },
      { "type",        DirUtils::fileTypeString(DirUtils::fileType(de)) },
      { "uid",         jsoncons::null_type() },
      { "gid",         jsoncons::null_type() },
      { "link",        "" },
      { "nlink",       DirUtils::nlink(de) },
      { "seq",         jsoncons::null_type() },
      { "attrs",       convertAttrs(de) },
      { "accessed",    jsoncons::null_type() },
      { "created",     jsoncons::null_type() },
      { "metadata",    jsoncons::null_type() },
//      { "modified",    DirUtils::mtime(de) },
      { "modified",    jsoncons::null_type() },
      { "deleted",     jsoncons::null_type() },
      { "backup",      jsoncons::null_type() },
      { "fn_accessed", jsoncons::null_type() },
      { "fn_created",  jsoncons::null_type() },
      { "fn_metadata", jsoncons::null_type() },
      { "fn_modified", jsoncons::null_type() }
    }
  );
}

jsoncons::json DirConverter::convertAttrs(const fs::directory_entry& de) const {
  return jsoncons::json(
    jsoncons::json_array_arg,
    {
      convertAttr(de)
    }
  );
}

jsoncons::json DirConverter::convertAttr(const fs::directory_entry& de) const {
  static const std::string flags = std::string(ATTR_FLAG_INUSE) + ", " + ATTR_FLAG_NONRES;

  return jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "id",            jsoncons::null_type() },
      { "flags",         flags },
      { "name",          jsoncons::null_type() },
      { "nrd_allocsize", DirUtils::fileSize(de) },
      { "type",          ATTR_TYPE_UNKNOWN }
    }
  );
}

jsoncons::json DirConverter::convertName(const fs::directory_entry& de) const {
  std::string sig_desc;
  std::vector<std::string> sig_tags;
  get_signature(de, &sig_desc, &sig_tags);
  return jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "name", de.path().filename().generic_string() },
      { "shrt_name", jsoncons::null_type() },
      { "meta_addr", jsoncons::null_type() },
      { "meta_seq",  jsoncons::null_type() },
      { "par_addr",  jsoncons::null_type() },
      { "par_seq",   jsoncons::null_type() },
//      { "date_added", name.date_added },
      { "type",      DirUtils::fileTypeString(DirUtils::fileType(de)) },
      { "flags",     NAME_FLAG_ALLOC },
      { "sig_desc",  sig_desc },
      { "sig_tags",  sig_tags }
    }
  );
}

struct lg_callback_context {
  const DirConverter* self;
  std::string ext;
  std::string* sig_desc;
  std::vector<std::string>* sig_tags;
  size_t min_hit_index;
};

void DirConverter::lg_callbackfn(void* userData, const LG_SearchHit* const hit) {
  auto ctx = (lg_callback_context*)userData;
  auto hit_info = lg_prog_pattern_info(ctx->self->lg.get_lg_prog(), hit->KeywordIndex);
  if (hit_info && hit_info->UserIndex < ctx->min_hit_index) {
    ctx->min_hit_index = hit_info->UserIndex;
  }
}

void DirConverter::get_signature(const fs::directory_entry& de, std::string* sig_desc, std::vector<std::string>* sig_tags) const {
  std::error_code ec;
  if (!de.is_regular_file(ec) || ec) {
    return;
  }
  std::ifstream ifs(de.path(), std::ios::binary);
  if (ifs) {
    auto ext = de.path().extension().u8string();
    if (!ext.empty()) {
      boost::algorithm::to_upper(ext);
      if (ext.length() > 1) {
        // clean dot
        ext = ext.substr(1);
      }
    }
    lg_callback_context ctx{ this, ext, sig_desc, sig_tags, std::numeric_limits<size_t>::max() };
    auto readed = ifs.readsome((char*)read_buf.data(), read_buf.size());
    if (lg.search(MemoryRegion(read_buf.data(), read_buf.data() + readed), &ctx, &DirConverter::lg_callbackfn)) {
      //
    }
    if (ctx.min_hit_index != std::numeric_limits<size_t>::max()) {
      // we got hit
      auto p = this->magics[ctx.min_hit_index];
      if (sig_tags) {
        *sig_tags = p.tags;
      }
      if (sig_desc) {
        if (p.extensions.count(ext)) {
          *sig_desc = p.extensions[ext];
        }
        else {
          *sig_desc = p.description;
        }
      }
      printf("%s, pattern hit(%lu): %s, desc %s, tags %s\n",
        de.path().c_str(), ctx.min_hit_index, p.pattern.c_str(), sig_desc->c_str(), sig_tags->size() ? sig_tags->at(0).c_str() : "");
    }
    else {
      // no hits? search manually
      // TODO
      // file_signatures.py 174 - 180
    }
  }
}


DirConverter::DirConverter() {
  std::string magics_file("./magics.json");
  SignatureUtil su;
  auto result = su.readMagics(magics_file);
  if (result.has_error()) {
    throw std::runtime_error("Couldn't open file: " + magics_file + std::string(", ") + result.error());
  }
  this->magics = result.value();

  auto r = lg.setup(this->magics);
  if (r.has_failure()) {
    throw std::runtime_error("LightGrep::setup failed: " + r.error());
  }

  auto max_read = r.value();
  read_buf.resize(max_read);

  // printf("max_read: %lu\n", max_read);

  // for (auto it = this->magics.begin(); it != this->magics.end(); ++it) {
  //   printf("value desc %s, pattern %s\n", it->description.c_str(), it->pattern.c_str());
  //   for (auto check : it->checks) {
  //     printf("\tcmp_type %d, offset %llu, value %lu\n", (int)check.compare_type, check.offset, check.value.size());
  //   }
  // }
}
