#include "readseek_impl.h"

class ReadSeek;

class PDFReader {
public:
  static std::string readTextFromPDF(ReadSeek& rs);
};