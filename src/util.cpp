#include "util.h"

#include <random>

std::string randomNumString() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 999999999);
  return std::to_string(dis(gen));
}

void printErrWithSource(const std::runtime_error& e, const std::string source) {
  std::cerr << source << ": " << e.what() << std::endl;
}
