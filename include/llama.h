#pragma once

#include "boost_asio.h"
#include "ducksig.h"
#include "filesignatures.h"
#include "llamaduck.h"
#include "options.h"
#include "rulereader.h"
#include "ruleengine.h"

struct ProgramHandle;

class Cli;
class InputReader;
class OutputWriter;

std::string readfile(const std::string& path);
std::string readDir(const std::string&);
bool readRulesFromDir(std::shared_ptr<LlamaRuleEngine> engine, const std::string& path);

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
  bool loadSignatures();

  void writeDB(const std::string& outdir);
  bool createDiskMap();
  bool generateDiskMapVisualization(const std::string& outputDir);

  std::shared_ptr<Cli> CliParser;

  boost::asio::thread_pool Pool;

  std::shared_ptr<Options> Opts;
  std::shared_ptr<ProgramHandle> LgProg;
  std::shared_ptr<InputReader> Input;
  std::shared_ptr<LlamaRuleEngine> RuleEngine;
  MagicsType SigMagics;
  std::shared_ptr<ProgramHandle> SigProg;
  // LlamaRuleEngine RuleEngine;
  LlamaDB Db;
  LlamaDBConnection DbConn;
};

