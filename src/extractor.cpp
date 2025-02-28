#include "extractor.h"
#include "readseek_impl.h"
#include "readseek_c_api.h"

std::unique_ptr<char, ExtractedTextDeleter> Extractor::extract_pdf(ReadSeek& rs) {
  RustReadSeekInterface rust_interface = { 
        .obj = &rs,
        .read = readseek_read,
        .seek = readseek_seek,
        .tellg = readseek_tellg,
        .size = readseek_size,
  };
  char* extracted_text = extract_text_from_pdf_readseek(rust_interface);
  std::unique_ptr<char, ExtractedTextDeleter> extracted_text_ptr(extracted_text);
  return extracted_text_ptr;
}