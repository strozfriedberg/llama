#pragma once

#include <memory>
#include <string>

class InputHandler;

class InputReader {
public:
  virtual ~InputReader() {}
  virtual void setInputHandler(std::shared_ptr<InputHandler> in) = 0;
  virtual bool startReading() = 0;

  static std::shared_ptr<InputReader> createTSK(const std::string& imgName);
  static std::shared_ptr<InputReader> createDir(const std::string& dirPath);
};
