#include "util.h"

#include <random>

std::string randomNumString() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 999999999);
  return std::to_string(dis(gen));
}

