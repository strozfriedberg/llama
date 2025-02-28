#pragma once

#include "readseek.h"

#include <libpdfextractor.h>

#include <memory>
#include <vector>

struct ExtractedTextDeleter {
    void operator()(char* ptr) const {
        free_extracted_text(ptr);
    }
};

class Extractor {
public:
  static std::unique_ptr<char, ExtractedTextDeleter> extract_pdf(ReadSeek& rs);
};