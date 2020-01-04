#include "outputbase.h"

#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include <boost/asio.hpp>

#include "filerecord.h"
#include "recordbuffer.h"

#include <iostream>

void closeAndFreeArchive(archive *a) {
  archive_write_close(a);
  archive_write_free(a);
}

class OutputTar : public OutputBase {
public:
  OutputTar(boost::asio::thread_pool &pool, const std::string &path);

  virtual ~OutputTar() {}

  virtual void outputFile(const FileRecord &rec) override {
    // std::cerr << "OutputTar::outputFile: " << rec.Path << std::endl;
    boost::asio::post(MainStrand, [=](){ writeFileRecord(rec); });
  }

  virtual void outputRecord(const FileRecord &rec) override {
    boost::asio::post(RecStrand, [=]() { FileRecBuf.write(rec.str()); });
  }

  virtual void outputRecords(const std::shared_ptr<std::vector<FileRecord>>& batch) override {
    boost::asio::post(RecStrand, [=]() { for (auto& rec: *batch) { FileRecBuf.write(rec.str()); } });
  }

  virtual void outputSearchHit(const std::string &) override {}

private:
  void writeFileRecord(const FileRecord &rec);

  boost::asio::strand<boost::asio::thread_pool::executor_type> MainStrand,
                                                               RecStrand;

  RecordBuffer FileRecBuf;

  std::string Path;

  std::shared_ptr<archive> Archive;
};

std::shared_ptr<OutputBase>
OutputBase::createTarWriter(boost::asio::thread_pool &pool,
                            const std::string &path) {
  return std::static_pointer_cast<OutputBase>(
      std::shared_ptr<OutputTar>(new OutputTar(pool, path)));
}

OutputTar::OutputTar(boost::asio::thread_pool &pool, const std::string &path)
    : MainStrand(pool.get_executor()), RecStrand(pool.get_executor()),
      FileRecBuf("recs/file_recs", 16 * 1024 * 1024, *this), Path(path),
      Archive(archive_write_new(), closeAndFreeArchive) {
  Path.append(".tar.lz4");
  archive_write_add_filter_lz4(Archive.get());
  archive_write_set_format_pax_restricted(Archive.get());
  archive_write_open_filename(Archive.get(), Path.c_str());

  // std::cerr << "Creating " << Path << std::endl;
}

void OutputTar::writeFileRecord(const FileRecord &rec) {
  // std::cerr << "Adding " << rec.Path << " to output tarball" << std::endl;
  std::shared_ptr<archive_entry> entry(archive_entry_new(), archive_entry_free);

  archive_entry_set_pathname(entry.get(), rec.Path.c_str());
  archive_entry_set_size(entry.get(), rec.Size);
  archive_entry_set_filetype(entry.get(), AE_IFREG);
  archive_entry_set_perm(entry.get(), 0644);
  archive_write_header(Archive.get(), entry.get());

  archive_write_data(Archive.get(), rec._data.c_str(), rec._data.size());
}
