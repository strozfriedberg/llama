#pragma once

#include <optional>
#include <stack>
#include <string>

#include "direntbatch.h"

class RecordHasher;

class DirentStack {
public:
  DirentStack(RecordHasher& rh): RecHasher(rh) {}

  bool empty() const;

  const Dirent& top() const;

  // Hashes the current dirent, adds that hash to the parent, then pops it
  Dirent pop();

  // Makes this dirent current. Returns the dirent immediately for "." and
  // ".." entries without modifying the path or stack.
  std::optional<Dirent> push(Dirent&& dirent);

private:
  struct Element {
    // The dirent record
    Dirent Rec;

    // The index of the last path separator
    size_t LastPathSepIndex;
  };

  std::stack<Element> Stack;
  std::string         Path;
  RecordHasher&       RecHasher;
};
