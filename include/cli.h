#pragma once

#include <iosfwd>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#if defined(__clang__)
  #pragma GCC diagnostic ignored "-Wdeprecated-builtins"
#endif
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <boost/program_options.hpp>
#pragma GCC diagnostic pop

#include "codec.h"
#include "options.h"

class Cli {
public:
  Cli();

  std::shared_ptr<Options> parse(int argc, const char* const argv[]) const;

  void printVersion(std::ostream& out) const;
  void printHelp(std::ostream& out) const;

private:
  std::string figureOutCommand(const boost::program_options::variables_map& optsMap) const;
  Codec figureOutCodec() const;

  void validateOpts() const;

  boost::program_options::options_description All;
  boost::program_options::positional_options_description PosOpts;

  std::shared_ptr<Options> Opts;
  std::string CodecSelect;
};

