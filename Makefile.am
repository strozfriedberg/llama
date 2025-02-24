ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir)/include -I$(srcdir)/src/xpdf -I$(srcdir)/src/splash -I$(srcdir)/src/goo -isystem $(BOOST_CPPFLAGS) $(LLAMA_CPPFLAGS)
AM_CXXFLAGS = $(STDCXX_CXXFLAGS) $(LLAMA_CXXFLAGS) $(BOOST_CXXFLAGS) $(ASAN_CXXFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) $(LLAMA_LDFLAGS) $(ASAN_LDFLAGS)

bin_PROGRAMS = src/llama

src_llama_common = \
	src/xpdf/AcroForm.cc \
	src/xpdf/Annot.cc \
	src/xpdf/Array.cc \
	src/xpdf/BuiltinFont.cc \
	src/xpdf/BuiltinFontTables.cc \
	src/xpdf/Catalog.cc \
	src/xpdf/CharCodeToUnicode.cc \
	src/xpdf/CMap.cc \
	src/xpdf/Decrypt.cc \
	src/xpdf/Dict.cc \
	src/xpdf/Error.cc \
	src/xpdf/FoFiBase.cc \
	src/xpdf/FoFiEncodings.cc \
	src/xpdf/FoFiIdentifier.cc \
	src/xpdf/FoFiTrueType.cc \
	src/xpdf/FoFiType1.cc \
	src/xpdf/FoFiType1C.cc \
	src/xpdf/FontEncodingTables.cc \
	src/xpdf/Function.cc \
	src/xpdf/Gfx.cc \
	src/xpdf/GfxFont.cc \
	src/xpdf/GfxState.cc \
	src/xpdf/GHash.cc \
	src/xpdf/GList.cc \
	src/xpdf/GlobalParams.cc \
	src/xpdf/gmem.cc \
	src/xpdf/gmempp.cc \
	src/xpdf/GString.cc \
	src/xpdf/JArithmeticDecoder.cc \
	src/xpdf/JBIG2Stream.cc \
	src/xpdf/JPXStream.cc \
	src/xpdf/Lexer.cc \
	src/xpdf/Link.cc \
	src/xpdf/NameToCharCode.cc \
	src/xpdf/Object.cc \
	src/xpdf/OptionalContent.cc \
	src/xpdf/Outline.cc \
	src/xpdf/OutputDev.cc \
	src/xpdf/Page.cc \
	src/xpdf/Parser.cc \
	src/xpdf/PDF417Barcode.cc \
	src/xpdf/PDFDoc.cc \
	src/xpdf/PDFDocEncoding.cc \
	src/xpdf/PSTokenizer.cc \
	src/xpdf/SecurityHandler.cc \
	src/xpdf/Stream.cc \
	src/xpdf/TextOutputDev.cc \
	src/xpdf/TextString.cc \
	src/xpdf/UnicodeMap.cc \
	src/xpdf/UnicodeRemapping.cc \
	src/xpdf/UnicodeTypeTable.cc \
	src/xpdf/UTF8.cc \
	src/xpdf/XFAScanner.cc \
	src/xpdf/XRef.cc \
	src/xpdf/Zoox.cc \
	src/goo/gfile.cc \
	src/batchhandler.cpp \
	src/blocksequence_impl.cpp \
	src/cli.cpp \
	src/dirconversion.cpp \
	src/direntbatch.cpp \
	src/direntstack.cpp \
	src/dirreader.cpp \
	src/fieldhasher.cpp \
	src/filerecord.cpp \
	src/filescheduler.cpp \
	src/filesignatures.cpp \
	src/fsm.cpp \
	src/hex.cpp \
	src/inodeandblocktrackerimpl.cpp \
	src/inputreader.cpp \
	src/lexer.cpp \
	src/llama.cpp \
	src/outputtar.cpp \
	src/parser.cpp \
	src/pdfreader.cpp \
	src/pooloutputhandler.cpp \
	src/processor.cpp \
	src/querybuilder.cpp \
	src/readseek_impl.cpp \
	src/recordbuffer.cpp \
	src/recordhasher.cpp \
	src/ruleengine.cpp \
	src/rulereader.cpp \
	src/schema.cpp \
	src/timestamps.cpp \
	src/treehasher.cpp \
	src/tskfacade.cpp \
	src/tskautowrapper.cpp \
	src/tskconversion.cpp \
	src/tskimgassembler.cpp \
	src/tskreader.cpp \
	src/tskreaderhelper.cpp \
	src/tsktimestamps.cpp \
	src/util.cpp

src_llama_SOURCES = $(src_llama_common) src/main.cpp

src_llama_CPPFLAGS = $(AM_CPPFLAGS) $(LG_CPPFLAGS) $(TSK_CPPFLAGS) $(LIBARCHIVE_CPPFLAGS) $(YARA_CPPFLAGS)
src_llama_LDADD = $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(YARA_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(DUCKDB_LIBS) $(STDCXX_LIB)

@VALGRIND_CHECK_RULES@

# unit tests and benchmarks are part of 'check' target
check_PROGRAMS = test/test
if HAS_YARA
check_PROGRAMS += test/benchmarks/benchmarks
endif

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER =

AM_TESTS_ENVIRONMENT = builddir=`pwd`;

# ...but don't run benchmarks automatically with 'check'
TESTS = test/test

test_test_SOURCES = \
	$(src_llama_common) \
	test/test_blocksequence.cpp \
	test/test_cli.cpp \
	test/test_dirconversion.cpp \
	test/test_direntstack.cpp \
	test/test_dirreader.cpp \
	test/test_duckdb.cpp \
	test/test_fieldhasher.cpp \
	test/test_fileproxy.cpp \
	test/test_filerecord.cpp \
	test/test_fsm.cpp \
	test/test_hex.cpp \
	test/test_inodeandblocktrackerimpl.cpp \
	test/test_llama.cpp \
	test/test_lexer.cpp \
	test/test_parser.cpp \
	test/test_patternparser.cpp \
	test/test_pdfreader.cpp \
	test/test_processor.cpp \
	test/test_querybuilder.cpp \
	test/test_readseek.cpp \
	test/test_recordbuffer.cpp \
	test/test_recordhasher.cpp \
	test/test_ruleengine.cpp \
	test/test_rulereader.cpp \
	test/test_tskconversion.cpp \
	test/test_tskimgassembler.cpp \
	test/test_tskreader.cpp \
	test/test_tskreaderhelper.cpp \
	test/test_tsktimestamps.cpp \
	test/test_util.cpp \
	test/test_filesignatures.cpp

test_test_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(CATCH2_CPPFLAGS)
test_test_LDADD = $(CATCH2_LIBS) $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(YARA_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(DUCKDB_LIBS) $(STDCXX_LIB)

test_benchmarks_benchmarks_SOURCES = \
  $(src_llama_common) \
  test/benchmarks/test_parser.cpp \
	test/benchmarks/test_yara.cpp

test_benchmarks_benchmarks_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(CATCH2_CPPFLAGS)
test_benchmarks_benchmarks_LDADD = $(CATCH2_LIBS) $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(YARA_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(DUCKDB_LIBS) $(STDCXX_LIB)
