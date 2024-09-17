#include "llama.h"

#include "batchhandler.h"
#include "cli.h"
#include "direntbatch.h"
#include "duckinode.h"
#include "duckhash.h"
#include "easyfut.h"
#include "filescheduler.h"
#include "inputhandler.h"
#include "inputreader.h"
#include "llamaduck.h"
#include "processor.h"
#include "timer.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <hasher/api.h>

#include <lightgrep/api.h>

namespace fs = std::filesystem;

Llama::Llama()
    : CliParser(std::make_shared<Cli>()), Pool(),
      LgProg(nullptr, lg_destroy_program),
      Reader(), Db(), DbConn(Db) {}

int Llama::run(int argc, const char* const argv[]) {
  try {
    Opts = CliParser->parse(argc, argv);
  }
  catch (const std::invalid_argument& arg) {
    std::cerr << "Error: " << arg.what() << '\n';
    return -1;
  }
  if ("help" == Opts->Command) {
    CliParser->printHelp(std::cout);
  }
  else if ("version" == Opts->Command) {
    CliParser->printVersion(std::cout);
  }
  else if ("search" == Opts->Command) {
    Timer overall(&std::cerr, "Overall time: ");
    search();
  }
  return 0;
}

void Llama::search() {
  if (init()) {
    Timer searchTime(&std::cerr, "Search time: ");
    // std::cout << "Number of patterns: " << lg_pattern_count(LgProg.get())
    //           << std::endl;
    std::filesystem::path outdir(Opts->Output);
    std::filesystem::create_directories(outdir);
//    auto out = std::shared_ptr<OutputWriter>(new OutputTar((outdir / "llama").string(), Opts->OutputCodec));
//    auto outh = std::shared_ptr<OutputHandler>(new PoolOutputHandler(Pool, DbConn, out));

    auto protoProc = std::make_shared<Processor>(&Db, LgProg);
    auto scheduler = std::make_shared<FileScheduler>(Db, Pool, protoProc, Opts);
    auto inh = std::shared_ptr<InputHandler>(new BatchHandler(scheduler));

    Input->setInputHandler(inh);
    //Input->setOutputHandler(outh);

    if (!Input->startReading()) {
      std::cerr << "startReading returned an error" << std::endl;
    }
    Pool.join();
    std::cerr << "Hashing Time: " << scheduler->getProcessorTime() << "s\n";

    writeDB(outdir.string());
    // std::cout << "All done" << std::endl;
  }
  else {
    std::cerr << "init returned false!" << std::endl;
  }
}

std::string readfile(const std::string& path) {
  std::ifstream f(path, std::ios::in);
  std::string str;
  f.seekg(0, std::ios::end);
  str.reserve(f.tellg());
  f.seekg(0, std::ios::beg);
  str.assign((std::istreambuf_iterator<char>(f)),
             std::istreambuf_iterator<char>());
  return str;
}

bool Llama::readpatterns(const std::vector<std::string>& keyFiles) {
  std::shared_ptr<FSMHandle> fsm(lg_create_fsm(1000, 100000), lg_destroy_fsm);

  const char* defaultEncodings[] = {"utf-8", "utf-16le"};
  LG_KeyOptions defaultKeyOpts{0, 0, 0};
  LG_Error *errs = nullptr;

  for (auto keyf : keyFiles) {
    // std::cerr << "add patterns from " << keyf << std::endl;

    std::string patterns = readfile(keyf);
    // std::cerr << "Patterns are:\n" << patterns << std::endl;
    int result = lg_add_pattern_list(fsm.get(), patterns.c_str(),
                                     keyf.c_str(), defaultEncodings, 2,
                                     &defaultKeyOpts, &errs);
    if (result < 0) {
      throw std::runtime_error("lg_add_pattern_list errored on file " + keyf);
    }
  }

  LG_ProgramOptions progOpts{1};
  LgProg = std::shared_ptr<ProgramHandle>(lg_create_program(fsm.get(), &progOpts), lg_destroy_program);
  if (LgProg) {
    return true;
  }
  else {
    // add some error-handling someday
    return false;
  }
}

bool Llama::openInput(const std::string& input) {
// FIXME: is_directory can throw
  Input = fs::is_directory(input) ?
    InputReader::createDir(input) :
    InputReader::createTSK(input);
  return bool(Input);
}

bool Llama::dbInit() {
  DuckDirent::createTable(DbConn.get(), "dirent");
  DuckInode::createTable(DbConn.get(), "inode");
  DuckHashRec::createTable(DbConn.get(), "hash");
  return true;
}

bool Llama::init() {
  Timer initTime(&std::cerr, "Init time: ");
  auto readPats = make_future(Pool, [this]() {
    return this->Opts->KeyFiles.size() ?
           readpatterns(this->Opts->KeyFiles): true;
  });

  auto open = make_future(Pool, [this]() {
    return openInput(this->Opts->Input);
  });
  
  auto db = make_future(Pool, [this]() {
    return dbInit();
  });

  if (!this->Opts->RuleFile.empty()) {
    std::string ruleStr = readfile(this->Opts->RuleFile);
    Reader.read(ruleStr);
  }

  return readPats.get() && open.get() && db.get();
}

void Llama::writeDB(const std::string& outdir) {
  Timer dbTime(&std::cerr, "DB write time: ");
  std::string query = "EXPORT DATABASE '";
  query += outdir;
  query += "' (FORMAT PARQUET);";
  duckdb_query(DbConn.get(), query.c_str(), nullptr);
}

