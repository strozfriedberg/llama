#pragma once

#include "inputhandler.h"
#include "filerecord.h"

#include <vector>

class MockInputHandler: public InputHandler {
public:
  virtual ~MockInputHandler() {}

  virtual void push(const Dirent& d) override {
    Dirents.push_back(d);
  }

  virtual void push(const Inode& i) override {
    Inodes.push_back(i);
  }

  virtual void maybeFlush() override {}

  virtual void flush() override {}

  std::vector<Dirent> Dirents;
  std::vector<Inode> Inodes;
};
