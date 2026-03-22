#pragma once

#include <sstream>
#include <stdexcept>

#define THROW(msg) \
  throw std::runtime_error(static_cast<const std::ostringstream&>( \
                             std::ostringstream() << __FILE__ << ":" << __LINE__ << ": " << msg) \
                             .str())

#define THROW_IF(condition, msg) \
  if (condition) \
  THROW(msg)

#include "evidenceioerror.h"

#define THROW_EVIDENCE(msg) \
  throw EvidenceIOError(static_cast<const std::ostringstream&>( \
                          std::ostringstream() << __FILE__ << ":" << __LINE__ << ": " << msg) \
                          .str())

#define THROW_EVIDENCE_IF(condition, msg) \
  if (condition) \
  THROW_EVIDENCE(msg)
