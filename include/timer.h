#pragma once

#include <chrono>
#include <ostream>

class Timer {
public:
  Timer() = default;

  Timer(std::ostream* outPtr, const char* msg) : OutPtr(outPtr), Message(msg) {}

  ~Timer() {
    if (OutPtr) {
      double e = elapsed();
      if (Message) {
        *OutPtr << Message;
      }
      *OutPtr << e << "s\n";
    }
  }

  double elapsed() const {
    return std::chrono::duration<double>(
      std::chrono::high_resolution_clock::now() - start
    ).count();
  }

private:
  const std::chrono::time_point<std::chrono::high_resolution_clock> start =
    std::chrono::high_resolution_clock::now();

  std::ostream* OutPtr = nullptr;
  const char* Message = nullptr;
};
