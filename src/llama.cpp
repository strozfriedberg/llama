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
#include "ruleengine.h"
#include "throw.h"
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
      RuleEngine(), Db(), DbConn(Db) {}

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
    std::filesystem::path outdir(Opts->Output);
    std::filesystem::create_directories(outdir);

    RuleEngine.createTables(DbConn);

    LG_ProgramOptions opts{10};
    LgProg.reset(lg_create_program(RuleEngine.buildFsm().getFsm(), &opts), lg_destroy_program);
    auto protoProc = std::make_shared<Processor>(&Db, LgProg, RuleEngine.patternToRuleId());
    auto scheduler = std::make_shared<FileScheduler>(Db, Pool, protoProc, Opts);
    auto inh = std::shared_ptr<InputHandler>(new BatchHandler(scheduler));

    Input->setInputHandler(inh);

    if (!Input->startReading()) {
      std::cerr << "startReading returned an error" << std::endl;
    }
    Pool.join();
    std::cerr << "Hashing Time: " << scheduler->getProcessorTime() << "s\n";

    RuleEngine.writeRulesToDb(DbConn);
    writeDB(outdir.string());
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

std::string readDir(const std::string& path) {
  const std::filesystem::path ruleDir{path};
  std::string str;
  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    str += readfile(file.path());
  }
  return str;
}

bool readRulesFromDir(LlamaRuleEngine& engine, const std::string& path) {
  const std::filesystem::path ruleDir{path};
  bool ret = false;

  if (!(std::filesystem::is_directory(path))) {
    std::cerr << "Error: " << path << " is not a directory" << std::endl;
    return false;
  }

  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    // don't exit early if there's an error because we want to give users all errors possible
    ret |= engine.read(readfile(file.path()), file.path());
  }
  return ret;
}

bool Llama::readpatterns(const std::vector<std::string>& keyFiles) {
  std::shared_ptr<FSMHandle> fsm(lg_create_fsm(1000, 100000), lg_destroy_fsm);

  const char* defaultEncodings[] = {"utf-8", "utf-16le"};
  LG_KeyOptions defaultKeyOpts{0, 0, 0};
  LG_Error *errs = nullptr;

  for (auto keyf : keyFiles) {
    std::string patterns = readfile(keyf);
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
  DBType<Dirent>::createTable(DbConn.get(), "dirent");
  DBType<Inode>::createTable(DbConn.get(), "inode");
  DBType<HashRec>::createTable(DbConn.get(), "hash");
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

  auto rules = make_future(Pool, [this](){
    return (this->Opts->RuleFile.empty() || RuleEngine.read(readfile(this->Opts->RuleFile), this->Opts->RuleFile)) &&
           (this->Opts->RuleDir.empty() || readRulesFromDir(RuleEngine, this->Opts->RuleDir));
  });

  return readPats.get() && open.get() && db.get() && rules.get();
}

void Llama::writeDB(const std::string& outdir) {
  Timer dbTime(&std::cerr, "DB write time: ");
  std::string query = "EXPORT DATABASE '";
  query += outdir;
  query += "' (FORMAT PARQUET);";
  duckdb_query(DbConn.get(), query.c_str(), nullptr);
}
