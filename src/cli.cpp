#include "cli.h"

#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <thread>

#include "throw.h"

namespace po = boost::program_options;

Cli::Cli() : All(), Opts(new Options) {
  // Command selection options
  po::options_description commands("Command selection -- in precedence order");
  commands.add_options()("help,h", "Display this help message")(
      "version,V", "Print version information and exit");

  po::options_description ioOpts("Input/Output Options");
  ioOpts.add_options()
    ("output", po::value<std::string>(&Opts->Output), "Directory to create for output")
    ("codec", po::value<std::string>(&CodecSelect)->default_value("lz4"), "Output tar compression method (none|gzip|lz4|lzma|bzip2|lzo|xz)")
    ("input", po::value<std::string>(&Opts->Input), "Evidence file or directory to process")
  ;

  po::options_description configOpts("Configuration Options");
  configOpts.add_options()
      ("rule-file,f",
        po::value<std::string>(&Opts->RuleFile)
        ->value_name("RULE_FILE"),
        "File containing Llama rule(s) to evaluate")
      ("rule-directory,F",
        po::value<std::string>(&Opts->RuleDir)
        ->value_name("RULE_DIR"),
        "Path to directory containing rule files")
      ("num-threads,j",
        po::value<unsigned int>(&Opts->NumThreads)
        ->default_value(std::thread::hardware_concurrency())
        ->value_name("THREADS"),
        "Number of worker threads to use")
      ("keywords-file,k",
        po::value<std::vector<std::string>>(&Opts->KeyFiles)
        ->composing()
        ->value_name("KEY_FILE"),
        "File containing newline-separated patterns to search");

  All.add(commands).add(ioOpts).add(configOpts);

  PosOpts.add("output", 1).add("input", -1);
}

std::shared_ptr<Options> Cli::parse(int argc, const char* const argv[]) const {
  po::variables_map optsMap;
  po::store(po::command_line_parser(argc, argv)
                .options(All)
                .positional(PosOpts)
                .run(),
            optsMap);
  po::notify(optsMap);

  Opts->Command = figureOutCommand(optsMap);
  Opts->OutputCodec = figureOutCodec();

  validateOpts();

  return Opts;
}

void Cli::printVersion(std::ostream& out) const {
  out << "llama - lightgrep's amazing media analyzer, pre-alpha " << __DATE__
      << std::endl;
}

void Cli::printHelp(std::ostream& out) const {
  printVersion(out);
  out << "\nUsage: llama [OPTIONS] OUTPUT_DIRECTORY INPUT_FILE\n"
      << All << std::endl;
}

std::string Cli::figureOutCommand(
  const boost::program_options::variables_map& optsMap) const {
  // convert some options to commands
  if (optsMap.count("help")) {
    return "help";
  }
  if (optsMap.count("version")) {
    return "version";
  }
  // default is "search"; check required args
  if (optsMap.count("output") == 0) {
    throw std::invalid_argument("No output file was specified");
  }
  if (optsMap.count("input") == 0) {
    throw std::invalid_argument("No input file/directory was specified");
  }
  return "search";
}

Codec Cli::figureOutCodec() const {
  if (0 == CodecSelect.compare("none")) {
    return Codec::NONE;
  }
  else if (0 == CodecSelect.compare("gzip")) {
    return Codec::GZIP;
  }
  else if (0 == CodecSelect.compare("lz4")) {
    return Codec::LZ4;
  }
  else if (0 == CodecSelect.compare("lzma")) {
    return Codec::LZMA;
  }
  else if (0 == CodecSelect.compare("bzip2")) {
    return Codec::BZIP2;
  }
  else if (0 == CodecSelect.compare("lzo")) {
    return Codec::LZOP;
  }
  else if (0 == CodecSelect.compare("xz")) {
    return Codec::XZ;
  }
  throw std::invalid_argument("'" + CodecSelect + "' is not a valid option for --codec");
}

void Cli::validateOpts() const {
  if (!Opts->RuleFile.empty()) {
    THROW_IF(!std::filesystem::exists(Opts->RuleFile), "Rule file " + Opts->RuleFile + " not found.");
    THROW_IF(!std::filesystem::is_regular_file(Opts->RuleFile), "Rule file " + Opts->RuleFile + " is not a file.");
  }

  if (!Opts->RuleDir.empty()) {
    THROW_IF(!std::filesystem::exists(Opts->RuleDir), "Rule directory " + Opts->RuleDir + " not found.");
    THROW_IF(!std::filesystem::is_directory(Opts->RuleDir), "Rule directory " + Opts->RuleDir + " is not a directory.");
  }
}
