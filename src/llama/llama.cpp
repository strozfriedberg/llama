#include "llama.h"

#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <streambuf>
#include <unordered_map>

#include <boost/filesystem.hpp>

template<typename ValueType>
struct easy_fut {
  easy_fut(): Promise(), Fut(Promise.get_future()) {}

  template<typename Callable>
  easy_fut(boost::asio::thread_pool& pool, Callable functor):
    Promise(), Fut(Promise.get_future())
  {
    run(pool, functor);
  }

  ValueType get() { return Fut.get(); }

  std::promise<ValueType> Promise;
  std::future<ValueType> Fut;

  template<typename Callable>
  void run(boost::asio::thread_pool& pool, Callable functor) {
    boost::asio::post(pool, [=]() {
      try {
        this->Promise.set_value(functor());
      }
      catch (...) {
        this->Promise.set_exception(std::current_exception());
      }
    });
  }
};

template<typename Callable>
auto make_future(boost::asio::thread_pool& pool, Callable functor) {
  return easy_fut<decltype(functor())>(pool, functor);
}

Llama::Llama():
  LgProg(nullptr, lg_destroy_program),
  Exec()
{}

int Llama::run(int argc, const char* const argv[]) {
  Opts = CliParser.parse(argc, argv);

  if (Opts) {
    if ("help" == Opts->Command) {
      CliParser.printHelp(std::cout);
    }
    else if ("version" == Opts->Command) {
      CliParser.printVersion(std::cout);
    }
    else if ("search" == Opts->Command) {
      search();
    }
    return 0;
  }
  return 1;
}

void Llama::search() {
  if (init()) {
    std::cout << "Number of patterns: " << lg_pattern_count(LgProg.get()) << std::endl;
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
  str.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return str;
}


bool Llama::readpatterns(const std::vector<std::string>& keyFiles) {
  // std::cerr << "begin readpatterns" << std::endl;
  std::shared_ptr<FSMHandle> fsm(lg_create_fsm(100000), lg_destroy_fsm);
  LgProg = std::shared_ptr<ProgramHandle>(lg_create_program(100000), lg_destroy_program);

  const char*  defaultEncodings[] = {"utf-8", "utf-16le"};
  LG_KeyOptions defaultKeyOpts{0, 0};
  LG_Error* errs = nullptr;

  for (auto keyf: keyFiles) {
    // std::cerr << "add patterns from " << keyf << std::endl;

    std::string patterns = readfile(keyf);
    // std::cerr << "Patterns are:\n" << patterns << std::endl;
    int result = lg_add_pattern_list(fsm.get(),
                                      LgProg.get(),
                                      patterns.c_str(),
                                      keyf.c_str(),
                                      defaultEncodings,
                                      2,
                                      &defaultKeyOpts,
                                      &errs);
    if (result < 0) {
      throw std::runtime_error("lg_add_pattern_list errored on file " + keyf);
    }
  }
  // std::cerr << "compiling program" << std::endl;
  LG_ProgramOptions progOpts{1};
  lg_compile_program(fsm.get(), LgProg.get(), &progOpts);
  // std::cerr << "Number of patterns: " << lg_pattern_count(LgProg.get()) << std::endl;
  // std::cerr << "Done with readpatterns" << std::endl;
  return true;
}


bool Llama::init() {
  if (Opts->KeyFiles.empty()) {
    return false;
  }
  auto readPats = make_future(Exec, [this](){ return readpatterns(this->Opts->KeyFiles); });
  return readPats.get();
}
