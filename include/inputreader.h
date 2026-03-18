#pragma once

#include <memory>
#include <string>

class InputHandler;
class OutputHandler;
class ProgressInfo;

class InputReader {
public:
  virtual ~InputReader() {}
  virtual void setInputHandler(const std::shared_ptr<InputHandler>& in) = 0;
  virtual void setOutputHandler(const std::shared_ptr<OutputHandler>& out) = 0;
  virtual void setProgressInfo(ProgressInfo*) {}
  virtual bool startReading() = 0;

  static std::shared_ptr<InputReader> createTSK(const std::string& imgName);
  static std::shared_ptr<InputReader> createDir(const std::string& dirPath);
#ifdef __linux__
  static std::shared_ptr<InputReader> createPosix(const std::string& mountpoint);
#endif
};
