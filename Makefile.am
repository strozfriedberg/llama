ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir)/include $(BOOST_CPPFLAGS) $(LLAMA_CPPFLAGS)
AM_CXXFLAGS = $(STDCXX_CXXFLAGS) $(LLAMA_CXXFLAGS) $(BOOST_CXXFLAGS) $(ASAN_CXXFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) $(LLAMA_LDFLAGS) $(ASAN_LDFLAGS)

bin_PROGRAMS = src/llama/llama

src_llama_llama_common = \
	src/llama/batchhandler.cpp \
	src/llama/blocksequence_impl.cpp \
	src/llama/cli.cpp \
	src/llama/dirconversion.cpp \
	src/llama/direntstack.cpp \
	src/llama/dirreader.cpp \
	src/llama/fieldhasher.cpp \
	src/llama/filerecord.cpp \
	src/llama/filescheduler.cpp \
	src/llama/hex.cpp \
	src/llama/inodeandblocktrackerimpl.cpp \
	src/llama/inputreader.cpp \
	src/llama/llama.cpp \
	src/llama/outputstream.cpp \
	src/llama/outputtar.cpp \
	src/llama/pooloutputhandler.cpp \
	src/llama/processor.cpp \
	src/llama/recordbuffer.cpp \
	src/llama/recordhasher.cpp \
	src/llama/schema.cpp \
	src/llama/timestamps.cpp \
	src/llama/treehasher.cpp \
	src/llama/tskfacade.cpp \
	src/llama/tskautowrapper.cpp \
	src/llama/tskconversion.cpp \
	src/llama/tskimgassembler.cpp \
	src/llama/tskreader.cpp \
	src/llama/tskreaderhelper.cpp \
	src/llama/tsktimestamps.cpp

src_llama_llama_SOURCES = $(src_llama_llama_common) src/llama/main.cpp

src_llama_llama_CPPFLAGS = $(AM_CPPFLAGS) $(LG_CPPFLAGS) $(TSK_CPPFLAGS) $(LIBARCHIVE_CPPFLAGS)
src_llama_llama_LDADD = $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(STDCXX_LIB)

@VALGRIND_CHECK_RULES@

check_PROGRAMS = test/llama/test

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER =

AM_TESTS_ENVIRONMENT = builddir=`pwd`;

TESTS = $(check_PROGRAMS)

test_llama_test_SOURCES = \
	$(src_llama_llama_common) \
	test/llama/test_blocksequence.cpp \
	test/llama/test_cli.cpp \
	test/llama/test_dirconversion.cpp \
	test/llama/test_direntstack.cpp \
	test/llama/test_dirreader.cpp \
	test/llama/test_fieldhasher.cpp \
	test/llama/test_fileproxy.cpp \
	test/llama/test_filerecord.cpp \
	test/llama/test_hex.cpp \
	test/llama/test_inodeandblocktrackerimpl.cpp \
	test/llama/test_processor.cpp \
	test/llama/test_recordbuffer.cpp \
	test/llama/test_recordhasher.cpp \
	test/llama/test_tskconversion.cpp \
	test/llama/test_tskimgassembler.cpp \
	test/llama/test_tskreader.cpp \
	test/llama/test_tskreaderhelper.cpp \
	test/llama/test_tsktimestamps.cpp

test_llama_test_CPPFLAGS = -I$(srcdir)/src/llama $(AM_CPPFLAGS) $(CATCH2_CPPFLAGS)
test_llama_test_LDADD = $(CATCH2_LIBS) $(LLAMA_LIBS) $(LG_LIBS) $(TSK_LIBS) $(LIBARCHIVE_LIBS) $(HASHER_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_ASIO_LIB) $(STDCXX_LIB)
