#pragma once

#include <memory>

struct SFHASH_Hasher;

struct ProgramHandle;
struct ContextHandle;

struct FileRecord;
class OutputHandler;


class Processor {
public:
  Processor(const std::shared_ptr<ProgramHandle>& prog);

  std::shared_ptr<Processor> clone() const;

  void process(FileRecord& rec, OutputHandler& out);

  Processor(const Processor&) = delete;

  double getProcessorTime() const { return ProcTimeTotal; }

private:
  std::shared_ptr<ProgramHandle> LgProg; // shared
  std::shared_ptr<ContextHandle> Ctx; // not shared, could be unique_ptr
  std::shared_ptr<SFHASH_Hasher> Hasher; // not shared, could be unique_ptr

  double ProcTimeTotal;
};

