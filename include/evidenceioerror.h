// ABOUTME: Exception type for evidence I/O failures (unreadable files, corrupt data)
// ABOUTME: Distinguished from application errors so Processor can catch and log them

#pragma once

#include <stdexcept>

class EvidenceIOError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
