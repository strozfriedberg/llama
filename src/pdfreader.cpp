#include <string>

#include "libpdfextractor.h"
#include "pdfreader.h"
#include "readseek_c_api.h"
#include "readseek_impl.h"

std::string PDFReader::readTextFromPDF(ReadSeek& rs) {
  RustReadSeekInterface rust_interface = { 
    .obj = &rs,
    .read = readseek_read,
    .seek = readseek_seek,
    .tellg = readseek_tellg,
    .size = readseek_size,
  };
  return extract_text_from_pdf_readseek(rust_interface);
}