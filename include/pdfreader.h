#pragma once

#include "readseek_impl.h"
#include "pdfextractor/libpdfextractor.h"

class ReadSeek;

class PDFReader {
public:
  PDFReader();
  ~PDFReader();

  bool readTextFromPDF(ReadSeek& rs);
  char* getExtractedText() { return ExtractedText; }

private:
  RustReadSeekInterface Interface;
  char* ExtractedText;
};