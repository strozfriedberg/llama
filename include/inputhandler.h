#pragma once

struct Dirent;
struct Inode;

class InputHandler {
public:
  virtual ~InputHandler() {}

  virtual void push(const Dirent&) = 0;
  virtual void push(const Inode&) = 0;

  virtual void maybeFlush() = 0;
  virtual void flush() = 0;
};
