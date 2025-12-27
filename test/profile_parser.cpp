// ABOUTME: Standalone program for profiling parser with Instruments
// ABOUTME: Generates rules, waits for user input, then parses once

#include <iostream>
#include <string>
#include "rulereader.h"
#include "benchmarks/rule_generator.h"

int main() {
  // Generate rules before profiling starts
  std::string corpus = generateRules(100);

  std::cerr << "Rules generated. Ready to profile.\n";
  std::cerr << "Attach Instruments and press Enter to continue...\n";

  // Wait for user input
  std::string line;
  std::getline(std::cin, line);

  std::cerr << "Starting parse...\n";

  // Parse once (this is what we profile)
  RuleReader r;
  bool success = r.read(corpus, "profile");

  std::cerr << "Parse complete. Rules parsed: " << r.getRules().size() << "\n";
  std::cerr << "Success: " << (success ? "true" : "false") << "\n";

  return success ? 0 : 1;
}
