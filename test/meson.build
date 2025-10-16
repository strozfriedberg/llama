# Common source files (excluding main.cpp from src)
llama_common_sources = [
  '../src/batchhandler.cpp',
  '../src/blocksequence_impl.cpp',
  '../src/cli.cpp',
  '../src/dirconversion.cpp',
  '../src/direntbatch.cpp',
  '../src/direntstack.cpp',
  '../src/dirreader.cpp',
  '../src/fieldhasher.cpp',
  '../src/filerecord.cpp',
  '../src/filescheduler.cpp',
  '../src/filesignatures.cpp',
  '../src/fsm.cpp',
  '../src/hashset.cpp',
  '../src/hex.cpp',
  '../src/inodeandblocktrackerimpl.cpp',
  '../src/inputreader.cpp',
  '../src/lexer.cpp',
  '../src/llama.cpp',
  '../src/outputtar.cpp',
  '../src/parser.cpp',
  '../src/pdfreader.cpp',
  '../src/pooloutputhandler.cpp',
  '../src/processor.cpp',
  '../src/querybuilder.cpp',
  '../src/readseek_c_api.cpp',
  '../src/readseek_impl.cpp',
  '../src/recordbuffer.cpp',
  '../src/recordhasher.cpp',
  '../src/ruleengine.cpp',
  '../src/rulereader.cpp',
  '../src/schema.cpp',
  '../src/timestamps.cpp',
  '../src/treehasher.cpp',
  '../src/tskfacade.cpp',
  '../src/tskautowrapper.cpp',
  '../src/tskconversion.cpp',
  '../src/tskimgassembler.cpp',
  '../src/tskreader.cpp',
  '../src/tskreaderhelper.cpp',
  '../src/tsktimestamps.cpp',
  '../src/util.cpp'
]

# Test source files
test_sources = [
  'test_blocksequence.cpp',
  'test_cli.cpp',
  'test_dirconversion.cpp',
  'test_direntstack.cpp',
  'test_dirreader.cpp',
  'test_duckdb.cpp',
  'test_fieldhasher.cpp',
  'test_fileproxy.cpp',
  'test_filerecord.cpp',
  'test_fsm.cpp',
  'test_hashset.cpp',
  'test_hex.cpp',
  'test_inodeandblocktrackerimpl.cpp',
  'test_llama.cpp',
  'test_lexer.cpp',
  'test_parser.cpp',
  'test_patternparser.cpp',
  'test_pdfreader.cpp',
  'test_processor.cpp',
  'test_querybuilder.cpp',
  'test_readseek.cpp',
  'test_recordbuffer.cpp',
  'test_recordhasher.cpp',
  'test_ruleengine.cpp',
  'test_rulereader.cpp',
  'test_tskconversion.cpp',
  'test_tskimgassembler.cpp',
  'test_tskreader.cpp',
  'test_tskreaderhelper.cpp',
  'test_tsktimestamps.cpp',
  'test_util.cpp',
  'test_filesignatures.cpp'
]

# Only build tests if Catch2 is available
if catch2_dep.found()

  # Main test executable
  test_exe = executable('llama_test',
    llama_common_sources + test_sources,
    include_directories: [inc, include_directories('../src')],
    dependencies: llama_deps + [catch2_dep],
    build_by_default: false
  )

  # Register the test with all sanitizers completely disabled
  test('llama_tests', test_exe,
       workdir : meson.project_source_root(),
       env : ['ASAN_OPTIONS=detect_leaks=0:detect_container_overflow=0:detect_stack_use_after_return=0:check_initialization_order=0:strict_init_order=0:halt_on_error=0:abort_on_error=0:disable_coredump=0:unmap_shadow_on_exit=1:mmap_limit_mb=0',
              'UBSAN_OPTIONS=halt_on_error=0:abort_on_error=0:print_stacktrace=0:silence_unsigned_overflow=1',
              'MSAN_OPTIONS=halt_on_error=0:abort_on_error=0:exit_code=0',
              'MALLOC_CHECK_=0'])

  # Benchmarks (only if YARA is available)
  if yara_dep.found()
    benchmark_sources = [
      'benchmarks/test_parser.cpp',
      'benchmarks/test_yara.cpp'
    ]

    benchmark_exe = executable('benchmarks',
      llama_common_sources + benchmark_sources,
      include_directories: [inc, include_directories('../src')],
      dependencies: llama_deps + [catch2_dep],
      build_by_default: false
    )
  endif

endif

