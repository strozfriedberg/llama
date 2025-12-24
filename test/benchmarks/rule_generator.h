#pragma once

#include <string>
#include <sstream>
#include <cstddef>

// Generates synthetic llama rules for benchmarking
inline std::string generateRules(size_t count) {
  std::ostringstream out;

  for (size_t i = 0; i < count; ++i) {
    out << "rule BenchmarkRule" << i << " {\n";
    out << "  meta:\n";
    out << "    description = \"Benchmark rule " << i << "\"\n";
    out << "    author = \"benchmark\"\n";

    // Add hash section to every 5th rule (must come before file_metadata)
    if (i % 5 == 0) {
      out << "  hash:\n";
      out << "    sha256 == \"";
      // Generate a fake but valid-looking sha256 (64 hex chars)
      for (int j = 0; j < 64; ++j) {
        out << "0123456789abcdef"[(i + j) % 16];
      }
      out << "\"\n";
    }

    out << "  file_metadata:\n";
    out << "    filesize > " << (i * 100 + 1000) << "\n";

    // Add grep section to every 3rd rule
    if (i % 3 == 0) {
      out << "  grep:\n";
      out << "    patterns:\n";
      out << "      p1 = \"pattern" << i << "\" fixed\n";
      out << "    condition:\n";
      out << "      any(p1)\n";
    }

    out << "}\n\n";
  }

  return out.str();
}
