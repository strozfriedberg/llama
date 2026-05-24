#include "llama.h"

#include "batchhandler.h"
#include "cli.h"
#include "direntbatch.h"
#include "diskmaphtml.h"
#include "duckexception.h"
#include "duckinode.h"
#include "duckbatch.h"
#include "duckhash.h"
#include "easyfut.h"
#include "evidencerec.h"
#include "extent.h"
#include "filescheduler.h"
#include "inputhandler.h"
#include "inputreader.h"
#include "llamaduck.h"
#ifdef __linux__
#include "posixreader.h"
#endif
#include "processor.h"
#include "progressinfo.h"
#include "progressthread.h"
#include "readseek_impl.h"
#include "ruleengine.h"
#include "throw.h"
#include "timer.h"
#include "tskreader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <hasher/api.h>

#include <lightgrep/api.h>

namespace fs = std::filesystem;

Llama::Llama()
    : CliParser(std::make_shared<Cli>()), Pool(),
      LgProg(nullptr, lg_destroy_program),
      RuleEngine(new LlamaRuleEngine()), Db(), DbConn(Db) {}

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
    try {
      search();
    }
    catch (const std::runtime_error &e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return -1;
    }
  }
  return 0;
}

void Llama::search() {
  if (init()) {
    Timer searchTime(&std::cerr, "Search time: ");
    std::filesystem::path outdir(Opts->Output);
    std::filesystem::create_directories(outdir);

    RuleEngine->createTables(DbConn);

    {
      LlamaDBAppender sigAppender(DbConn.get(), "signatures");
      SigBatch sigBatch;
      for (const auto& m : SigMagics) {
        sigBatch.add(SigRec{m->Id, m->Name, m->Description});
      }
      sigBatch.copyToDB(sigAppender.get());
      sigAppender.flush();
    }

    ProgressInfo progressInfo;

    LG_ProgramOptions opts{10};
    LgProg.reset(lg_create_program(RuleEngine->buildFsm().getFsm(), &opts), lg_destroy_program);
    auto procContext = std::make_shared<ProcessorContext>(&Db, LgProg, RuleEngine, Opts->ExclusionHashset, Opts->InclusionHashset, SigMagics, Plugins, SigProg, &progressInfo);
    auto protoProc = std::make_shared<Processor>(procContext);

    ProgressThread progressThread(progressInfo, isatty(STDERR_FILENO));
    progressThread.start();

    for (size_t i = 0; i < Opts->Inputs.size(); ++i) {
      // Input is already open (from init() or previous iteration's prefetch)

      // Prefetch next input in background
      std::shared_ptr<InputReader> nextInput;
      std::unique_ptr<easy_fut<bool>> nextFuture;
      if (i + 1 < Opts->Inputs.size()) {
        nextFuture = std::make_unique<easy_fut<bool>>(Pool, [this, i, &nextInput]() {
          nextInput = openInput(this->Opts->Inputs[i + 1]);
          return bool(nextInput);
        });
      }

      std::string evidenceName = std::filesystem::path(Opts->Inputs[i]).filename().string();
      progressInfo.setEvidenceFile(evidenceName, i + 1, Opts->Inputs.size());

      auto scheduler = std::make_shared<FileScheduler>(Db, Pool, protoProc, Opts);
      auto inh = std::shared_ptr<InputHandler>(new BatchHandler(scheduler));

      Input->setInputHandler(inh);
      Input->setProgressInfo(&progressInfo);

#ifdef __linux__
      if (auto posixReader = std::dynamic_pointer_cast<PosixReader>(Input)) {
        auto extentAppender = std::make_shared<LlamaDBAppender>(DbConn.get(), "extents");
        posixReader->setExtentAppender(extentAppender);
      }
#endif

      if (!Input->startReading()) {
        std::cerr << "\r\033[K" << "startReading returned an error for " << Opts->Inputs[i] << std::endl;
      }

      // Wait for all processing to complete
      scheduler->getCompletionFuture().get();

      // Write assembler records to DB (evidence_files, volumes, filesystems)
      if (auto tskReader = dynamic_cast<TskReader*>(Input.get())) {
        writeEvidenceRecords(tskReader->getAssembler());
      }

      if (progressInfo.exceptionCount() > 0) {
        std::cerr << "\r\033[K" << progressInfo.exceptionCount()
                  << " evidence I/O exceptions encountered -- see exception_log table\n";
      }

      std::cerr << "\r\033[K" << "Hashing Time (" << evidenceName << "): " << scheduler->getProcessorTime() << "s\n";

      // Wait for next input to be ready, swap
      if (nextFuture) {
        nextFuture->get();
        Input = nextInput;
      }
    }

    progressThread.stop();
    Pool.join();  // All evidence files processed -- terminate pool threads

    Plugins.shutdown();

    RuleEngine->writeRulesToDb(DbConn);

#ifdef __linux__
    if (std::dynamic_pointer_cast<PosixReader>(Input)) {
      std::cerr << "Creating disk map from extents..." << std::endl;
      if (createDiskMap()) {
        std::string vizDir = outdir.string() + "/diskmap";
        std::cerr << "Generating disk map visualization..." << std::endl;
        generateDiskMapVisualization(vizDir);
      }
    }
#endif

    writeDB(outdir.string());
    writeReports(outdir.string());
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

bool readRulesFromDir(std::shared_ptr<LlamaRuleEngine> engine, const std::string& path) {
  std::filesystem::path ruleDir{path};
  bool ret = true;

  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    std::string filePath = file.path().string();
    if (!engine->read(readfile(filePath), filePath)) {
      ret = false;
      // Print lexer errors with filename context
      for (const auto& err : engine->getReader().getLexer().errors()) {
        std::cerr << filePath << ": " << err.what() << '\n';
      }
      // Print parser errors with filename context
      for (const auto& err : engine->getReader().getParser().errors()) {
        std::cerr << filePath << ": " << err.what() << '\n';
      }
    }
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

std::shared_ptr<InputReader> Llama::openInput(const std::string& input) {
// FIXME: is_directory can throw
  std::shared_ptr<InputReader> reader;
  if (fs::is_directory(input)) {
#ifdef __linux__
    reader = InputReader::createPosix(input);
#else
    reader = InputReader::createDir(input);
#endif
  } else {
    reader = InputReader::createTSK(input);
  }
  return reader;
}

bool Llama::dbInit() {
  DBType<Dirent>::createTable(DbConn.get(), "dirent");
  DBType<Inode>::createTable(DbConn.get(), "inode");
  DBType<HashRec>::createTable(DbConn.get(), "hash");
  DBType<Extent>::createTable(DbConn.get(), "extents");
  DBType<BatchRec>::createTable(DbConn.get(), "batches");
  DBType<ExceptionRecord>::createTable(DbConn.get(), "exception_log");
  DBType<EvidenceFileRec>::createTable(DbConn.get(), "evidence_files");
  DBType<VolumeRec>::createTable(DbConn.get(), "volumes");
  DBType<FilesystemRec>::createTable(DbConn.get(), "filesystems");
  return true;
}

void Llama::writeEvidenceRecords(const TskImgAssembler& assembler) {
  // Evidence file
  {
    LlamaDBAppender appender(DbConn.get(), "evidence_files");
    EvidenceFileBatch batch;
    batch.add(assembler.evidenceFile());
    batch.copyToDB(appender.get());
    appender.flush();
  }

  // Volumes
  if (!assembler.volumes().empty()) {
    LlamaDBAppender appender(DbConn.get(), "volumes");
    VolumeBatch batch;
    for (const auto& vol : assembler.volumes()) {
      batch.add(vol);
    }
    batch.copyToDB(appender.get());
    appender.flush();
  }

  // Filesystems
  if (!assembler.filesystems().empty()) {
    LlamaDBAppender appender(DbConn.get(), "filesystems");
    FilesystemBatch batch;
    for (const auto& fs : assembler.filesystems()) {
      batch.add(fs);
    }
    batch.copyToDB(appender.get());
    appender.flush();
  }
}

bool Llama::loadSignatures() {
  std::shared_ptr<FILE> sigFilePtr(std::fopen(Opts->SignaturesPath.c_str(), "rb"), std::fclose);
  if (!sigFilePtr) {
    std::cerr << "Error opening signatures file: " << Opts->SignaturesPath << std::endl;
    return false;
  }
  ReadSeekFile sigFile(sigFilePtr);
  SigMagics = FileSigAnalyzer::readMagics(sigFile);

  if (SigMagics.empty()) {
    return true;
  }

  // Try to load cached compiled program if it's newer than the signatures file
  namespace fs = std::filesystem;
  auto homeDir = std::getenv("HOME");
  std::string cachePath;
  if (homeDir) {
    cachePath = std::string(homeDir) + "/.llama/cache/magics.lgp";
  }

  if (!cachePath.empty() && fs::exists(cachePath)) {
    auto sigsMtime = fs::last_write_time(Opts->SignaturesPath);
    auto cacheMtime = fs::last_write_time(cachePath);
    if (cacheMtime > sigsMtime) {
      try {
        SigProg = Lightgrep::readProgram(cachePath);
        return true;
      }
      catch (const std::exception& e) {
        std::cerr << "Warning: failed to read cached signatures: " << e.what() << std::endl;
        // Fall through to recompile
      }
    }
  }

  SigProg = Lightgrep::compile(SigMagics);

  // Write cache (failure is non-fatal)
  if (!cachePath.empty() && SigProg) {
    std::error_code ec;
    fs::create_directories(std::string(homeDir) + "/.llama/cache", ec);
    try {
      Lightgrep::writeProgram(SigProg, cachePath);
    }
    catch (const std::exception& e) {
      std::cerr << "Warning: failed to cache compiled signatures: " << e.what() << std::endl;
    }
  }

  return true;
}

bool Llama::loadPlugins() {
  if (Opts->PluginDir.empty()) {
    return true;
  }
  Plugins.loadPlugins(Opts->PluginDir, DbConn.get());
  if (Plugins.pluginCount() == 0) {
    std::cerr << "Warning: no plugins found in " << Opts->PluginDir << "\n";
  }
  return true;
}

bool Llama::init() {
  Timer initTime(&std::cerr, "Init time: ");
  auto readPats = make_future(Pool, [this]() {
    return this->Opts->KeyFiles.size() ?
           readpatterns(this->Opts->KeyFiles): true;
  });

  auto open = make_future(Pool, [this]() {
    Input = openInput(this->Opts->Inputs[0]);
    return bool(Input);
  });

  auto db = make_future(Pool, [this]() {
    return dbInit();
  });

  auto rules = make_future(Pool, [this](){
    return (this->Opts->RuleFile.empty() || RuleEngine->read(readfile(this->Opts->RuleFile), this->Opts->RuleFile)) &&
           (this->Opts->RuleDir.empty() || readRulesFromDir(RuleEngine, this->Opts->RuleDir));
  });

  auto sigs = make_future(Pool, [this]() {
    return loadSignatures();
  });

  bool ok = readPats.get() && open.get() && db.get() && rules.get() && sigs.get();
  if (ok) {
    ok = loadPlugins();
  }
  return ok;
}

void Llama::writeReports(const std::string& outdir) {
  auto writeFile = [](const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
  };

  // File inventory: one row per file with aggregated signatures
  writeFile(outdir + "/file_inventory.sql",
    "INSTALL spatial;\nLOAD spatial;\n"
    "COPY (\n"
    "  SELECT\n"
    "    ef.Name AS EvidenceFile,\n"
    "    d.Path || d.Name AS FullPath,\n"
    "    d.Name,\n"
    "    d.Type AS DirentType,\n"
    "    d.Flags AS DirentFlags,\n"
    "    i.Type AS InodeType,\n"
    "    i.Flags AS InodeFlags,\n"
    "    CAST(i.Filesize AS BIGINT) AS Filesize,\n"
    "    i.Created,\n"
    "    i.Modified,\n"
    "    i.Accessed,\n"
    "    i.Metadata AS MetadataChanged,\n"
    "    h.MD5,\n"
    "    h.SHA1,\n"
    "    h.SHA256,\n"
    "    STRING_AGG(DISTINCT s.Name, ', ' ORDER BY s.Name) AS Signatures,\n"
    "    CAST(d.MetaAddr AS BIGINT) AS MetaAddr,\n"
    "    CAST(d.ParentAddr AS BIGINT) AS ParentAddr\n"
    "  FROM '" + outdir + "/dirent.parquet' d\n"
    "  JOIN '" + outdir + "/inode.parquet' i ON d.MetaId = i.Id\n"
    "  LEFT JOIN '" + outdir + "/evidence_files.parquet' ef ON i.EvidenceFileName = ef.Name\n"
    "  LEFT JOIN '" + outdir + "/hash.parquet' h ON d.MetaId = h.InodeId\n"
    "  LEFT JOIN '" + outdir + "/file_signatures.parquet' fs ON h.SHA256 = fs.FileHash\n"
    "  LEFT JOIN '" + outdir + "/signatures.parquet' s ON fs.SigId = s.Id\n"
    "  GROUP BY ALL\n"
    "  ORDER BY ef.Name, FullPath\n"
    ") TO '" + outdir + "/file_inventory.xlsx'\n"
    "WITH (FORMAT GDAL, DRIVER 'xlsx');\n"
  );

  // Rule hits: which rules matched which files
  writeFile(outdir + "/rule_hits_report.sql",
    "INSTALL spatial;\nLOAD spatial;\n"
    "COPY (\n"
    "  SELECT\n"
    "    i.EvidenceFileName AS EvidenceFile,\n"
    "    r.name AS RuleName,\n"
    "    rh.path || rh.name AS FullPath,\n"
    "    rh.name AS FileName,\n"
    "    rh.inode_id AS InodeId\n"
    "  FROM '" + outdir + "/rule_hits.parquet' rh\n"
    "  JOIN '" + outdir + "/rules.parquet' r ON rh.id = r.id\n"
    "  LEFT JOIN '" + outdir + "/inode.parquet' i ON rh.inode_id = i.Id\n"
    "  ORDER BY i.EvidenceFileName, r.name, rh.path, rh.name\n"
    ") TO '" + outdir + "/rule_hits.xlsx'\n"
    "WITH (FORMAT GDAL, DRIVER 'xlsx');\n"
  );

  // Search hits: pattern matches with file context
  writeFile(outdir + "/search_hits_report.sql",
    "INSTALL spatial;\nLOAD spatial;\n"
    "COPY (\n"
    "  SELECT\n"
    "    i.EvidenceFileName AS EvidenceFile,\n"
    "    r.name AS RuleName,\n"
    "    d.Path || d.Name AS FullPath,\n"
    "    d.Name AS FileName,\n"
    "    sh.pattern AS Pattern,\n"
    "    CAST(sh.start_offset AS BIGINT) AS StartOffset,\n"
    "    CAST(sh.end_offset AS BIGINT) AS EndOffset,\n"
    "    CAST(sh.length AS BIGINT) AS Length\n"
    "  FROM '" + outdir + "/search_hits.parquet' sh\n"
    "  JOIN '" + outdir + "/hash.parquet' h ON sh.file_hash = h.SHA256\n"
    "  JOIN '" + outdir + "/dirent.parquet' d ON h.InodeId = d.MetaId\n"
    "  LEFT JOIN '" + outdir + "/inode.parquet' i ON d.MetaId = i.Id\n"
    "  LEFT JOIN '" + outdir + "/rules.parquet' r ON sh.rule_id = r.id\n"
    "  ORDER BY i.EvidenceFileName, r.name, d.Path, d.Name, sh.start_offset\n"
    ") TO '" + outdir + "/search_hits.xlsx'\n"
    "WITH (FORMAT GDAL, DRIVER 'xlsx');\n"
  );
}

void Llama::writeDB(const std::string& outdir) {
  Timer dbTime(&std::cerr, "DB write time: ");
  std::string query = "EXPORT DATABASE '";
  query += outdir;
  query += "' (FORMAT PARQUET);";
  duckdb_query(DbConn.get(), query.c_str(), nullptr);
}

bool Llama::createDiskMap() {
  duckdb_result result;

  // Step 1: Create boundaries table
  auto state = duckdb_query(DbConn.get(),
    "CREATE TEMP TABLE boundaries AS "
    "SELECT DISTINCT PhysicalStart AS pos FROM extents "
    "UNION "
    "SELECT DISTINCT PhysicalEnd AS pos FROM extents "
    "ORDER BY pos;",
    &result);

  if (state == DuckDBError) {
    std::cerr << "Error creating boundaries table: " << duckdb_result_error(&result) << "\n";
    duckdb_destroy_result(&result);
    return false;
  }
  duckdb_destroy_result(&result);

  // Step 2: Create intervals table
  state = duckdb_query(DbConn.get(),
    "CREATE TEMP TABLE intervals AS "
    "SELECT start, \"end\" FROM ("
    "  SELECT "
    "    pos AS start, "
    "    LEAD(pos) OVER (ORDER BY pos) AS \"end\" "
    "  FROM boundaries"
    ") WHERE \"end\" IS NOT NULL;",
    &result);

  if (state == DuckDBError) {
    std::cerr << "Error creating intervals table: " << duckdb_result_error(&result) << "\n";
    duckdb_destroy_result(&result);
    return false;
  }
  duckdb_destroy_result(&result);

  // Step 3: Create diskmap table with claimants
  state = duckdb_query(DbConn.get(),
    "CREATE TABLE diskmap AS "
    "SELECT "
    "    i.start AS PhysicalStart, "
    "    i.\"end\" AS PhysicalEnd, "
    "    string_agg(e.Path, ',') AS Claimants "
    "FROM intervals i "
    "LEFT JOIN extents e "
    "    ON e.PhysicalStart <= i.start "
    "    AND e.PhysicalEnd >= i.\"end\" "
    "GROUP BY i.start, i.\"end\" "
    "ORDER BY i.start;",
    &result);

  if (state == DuckDBError) {
    std::cerr << "Error creating diskmap table: " << duckdb_result_error(&result) << "\n";
    duckdb_destroy_result(&result);
    return false;
  }
  duckdb_destroy_result(&result);

  std::cerr << "Disk map created successfully\n";
  return true;
}

bool Llama::generateDiskMapVisualization(const std::string& outputDir) {
  DiskMapHtmlGenerator generator(DbConn, 4096);
  if (!generator.generate(outputDir)) {
    std::cerr << "Error generating disk map visualization\n";
    return false;
  }
  std::cerr << "Disk map visualization written to " << outputDir << "\n";
  return true;
}
