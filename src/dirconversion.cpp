#include "dirconversion.h"

#include "schema.h"

#include <system_error>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
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

    lg_callback_context ctx{ this, std::numeric_limits<size_t>::max() };
    auto readed = ifs.readsome((char*)read_buf.data(), read_buf.size());
    auto lg_err = lg.search(MemoryRegion(read_buf.data(), read_buf.data() + readed), &ctx, &DirConverter::lg_callbackfn);
    if (lg_err.has_error()) {
      throw std::runtime_error("lg.search() failed on file: " + de.path().string() + std::string(", error: ") + lg_err.error());
    }
    auto signature_hit = [&sig_tags, &sig_desc, &ext](magic const& m) {
      if (sig_tags) {
        *sig_tags = m.tags;
      }
      if (sig_desc) {
        if (m.extensions.count(ext)) {
          *sig_desc = m.extensions.at(ext);
        }
        else {
          *sig_desc = m.description;
        }
      }
      };
    if (ctx.min_hit_index != std::numeric_limits<size_t>::max()) {
      // hit
      auto p = *this->magics[ctx.min_hit_index];
      signature_hit(p);
      return;
    }

    // no hits? search manually

    Binary check_buf(read_buf);

    auto get_buf = [&ifs, this, &check_buf](Offset const& offset, std::size_t size) {
      if (size + offset.count > read_buf.size() || offset.from_start == false) {
        check_buf.resize(size);
        ifs.seekg(offset.count, offset.from_start ? std::ios_base::beg : std::ios_base::end);
        auto readed = ifs.readsome((char*)check_buf.data(), check_buf.size());
        if (readed != (std::streamsize)size)
          throw std::runtime_error(("readsome(" + std::to_string(size) + ") at " + std::to_string(offset.count) + ", ", std::to_string(offset.from_start) + " failed."));
        return check_buf;
      }
      return Binary(&read_buf[offset.count], &read_buf[offset.count + size]);
      };

    // by "ext"
    auto s = signature_dict.find(ext);
    if (s != signature_dict.end()) {
      auto m = *s->second;
      // by default if checks is empty - make a hit
      bool all_checks_passed = true;
      BOOST_FOREACH(auto check_it, m.checks) {
        if (!(all_checks_passed = (check_it.compare(get_buf(check_it.offset, check_it.value.size())) == true)))
          break;
      }
      if (all_checks_passed) {
        // hit
        signature_hit(m);
        return;
      }
    }

    // final check through all signatures
    for (auto const& s : signature_list) {
      bool all_checks_passed = false;
      BOOST_FOREACH(auto check_it, s->checks) {
        if (!(all_checks_passed = (check_it.compare(get_buf(check_it.offset, check_it.value.size())) == true)))
          break;
      }
      if (all_checks_passed) {
        signature_hit(*s);
        return;
      }
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

  auto magics = result.value();

  // fill signature_dict & signature_list
  for (auto const& m : magics) {
    signature_list.push_back(m);
    for (auto const& ext : m->extensions) {
      if (signature_dict.count(ext.first) == 0) {
        signature_dict.insert(std::pair(ext.first, m));
      }
    }
  }

  // resort magics by pattern size in desceding order ('bigger' patterns first)
  std::sort(begin(magics), end(magics), [](std::shared_ptr<magic> const& a, std::shared_ptr<magic> const& b) -> bool {
    return a->get_pattern_length(true) > b->get_pattern_length(true);
    });

  this->magics = std::move(magics);

  auto r = lg.setup(this->magics);
  if (r.has_failure()) {
    throw std::runtime_error("LightGrep::setup failed: " + r.error());
  }

  auto max_read = r.value();
  read_buf.resize(max_read);
}

