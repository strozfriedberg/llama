// ABOUTME: Declares formatTimestamp() for converting Unix timestamps to strings
// ABOUTME: Primary signature uses an out-parameter for string reuse; convenience wrapper returns by value

#pragma once

#include <cstdint>
#include <string>

// Primary: caller passes a string for reuse across calls
void formatTimestamp(int64_t unix_time, uint64_t ns, std::string& out);

// Convenience: returns a new string
std::string formatTimestamp(int64_t unix_time, uint64_t ns);

// Returns the current UTC time as an ISO 8601 string (e.g., "2026-03-22T15:30:45")
std::string nowISO();
