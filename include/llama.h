#pragma once

#include "boost_asio.h"
#include "llamaduck.h"
#include "options.h"
#include "rulereader.h"

struct ProgramHandle;

class Cli;
class InputReader;
class OutputWriter;

class Llama {
public:
  Llama();

  int run(int argc, const char* const argv[]);

  void search();

  bool init();

private:
  bool readpatterns(const std::vector<std::string>& keyFiles);
  bool openInput(const std::string& input);
  bool dbInit();

  void writeDB(const std::string& outdir);
  void makeRuleTable();

  std::shared_ptr<Cli> CliParser;

  boost::asio::thread_pool Pool;

  std::shared_ptr<Options> Opts;
  std::shared_ptr<ProgramHandle> LgProg;
  std::shared_ptr<InputReader> Input;

  RuleReader Reader;
  LlamaDB Db;
  LlamaDBConnection DbConn;
};

