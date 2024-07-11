ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir)/include $(BOOST_CPPFLAGS) $(LLAMA_CPPFLAGS)
AM_CXXFLAGS = $(STDCXX_CXXFLAGS) $(LLAMA_CXXFLAGS) $(BOOST_CXXFLAGS) $(ASAN_CXXFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) $(LLAMA_LDFLAGS) $(ASAN_LDFLAGS)

bin_PROGRAMS = src/llama

src_llama_common = \
	src/batchhandler.cpp \
	src/blocksequence_impl.cpp \
	src/cli.cpp \
	src/dirconversion.cpp \
	src/direntstack.cpp \
	src/dirreader.cpp \
	src/fieldhasher.cpp \
	src/filerecord.cpp \
	src/filescheduler.cpp \
	src/hex.cpp \
	src/inodeandblocktrackerimpl.cpp \
	src/inputreader.cpp \
	src/lexer.cpp \
	src/llama.cpp \
	src/outputtar.cpp \
	src/pooloutputhandler.cpp \
	src/processor.cpp \
	src/readseek_impl.cpp \
	src/recordbuffer.cpp \
	src/recordhasher.cpp \
	src/schema.cpp \
	src/timestamps.cpp \
	src/treehasher.cpp \
	src/tskfacade.cpp \
	src/tskautowrapper.cpp \
	src/tskconversion.cpp \
	src/tskimgassembler.cpp \
	src/tskreader.cpp \
	src/tskreaderhelper.cpp \
	src/tsktimestamps.cpp

src_llama_SOURCES = $(src_llama_common) src/main.cpp

src_llama_CPPFLAGS = $(AM_CPPFLAGS) $(LG_CPPFLAGS) $(TSK_CPPFLAGS) $(LIBARCHIVE_CPPFLAGS) $(YARA_CPPFLAGS)
src_llama_LDADD = $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(YARA_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(STDCXX_LIB)

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
	test/test_fieldhasher.cpp \
	test/test_fileproxy.cpp \
	test/test_filerecord.cpp \
	test/test_hex.cpp \
	test/test_inodeandblocktrackerimpl.cpp \
	test/test_lexer.cpp \
	test/test_processor.cpp \
	test/test_readseek.cpp \
	test/test_recordbuffer.cpp \
	test/test_recordhasher.cpp \
	test/test_tskconversion.cpp \
	test/test_tskimgassembler.cpp \
	test/test_tskreader.cpp \
	test/test_tskreaderhelper.cpp \
	test/test_tsktimestamps.cpp

test_test_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(CATCH2_CPPFLAGS)
test_test_LDADD = $(CATCH2_LIBS) $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(YARA_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(STDCXX_LIB)

test_benchmarks_benchmarks_SOURCES = \
	test/benchmarks/test_yara.cpp

test_benchmarks_benchmarks_CPPFLAGS = -I$(srcdir)/src $(AM_CPPFLAGS) $(CATCH2_CPPFLAGS)
test_benchmarks_benchmarks_LDADD = $(CATCH2_LIBS) $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(YARA_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(STDCXX_LIB)
