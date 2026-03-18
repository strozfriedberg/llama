# Source files for the main executable
llama_sources = [
  'batchhandler.cpp',
  'blocksequence_impl.cpp',
  'cli.cpp',
  'dirconversion.cpp',
  'direntbatch.cpp',
  'direntstack.cpp',
  'dirreader.cpp',
  'diskmaphtml.cpp',
  'fieldhasher.cpp',
  'filerecord.cpp',
  'filescheduler.cpp',
  'filesignatures.cpp',
  'fsm.cpp',
  'hashset.cpp',
  'hex.cpp',
  'inodeandblocktrackerimpl.cpp',
  'inputreader.cpp',
  'lexer.cpp',
  'llama.cpp',
  'main.cpp',
  'outputtar.cpp',
  'parser.cpp',
  'pdfreader.cpp',
  'pooloutputhandler.cpp',
  'processor.cpp',
  'progressinfo.cpp',
  'querybuilder.cpp',
  'readseek_c_api.cpp',
  'readseek_impl.cpp',
  'recordbuffer.cpp',
  'recordhasher.cpp',
  'ruleengine.cpp',
  'rulereader.cpp',
  'schema.cpp',
  'timestamps.cpp',
  'treehasher.cpp',
  'tskfacade.cpp',
  'tskautowrapper.cpp',
  'tskconversion.cpp',
  'tskimgassembler.cpp',
  'tskreader.cpp',
  'tskreaderhelper.cpp',
  'tsktimestamps.cpp',
  'util.cpp'
]

# Add Linux-specific sources
if host_machine.system() == 'linux'
    llama_sources += files('posixreader.cpp')
endif

# Build the llama executable
llama_exe = executable('llama',
  llama_sources,
  include_directories: inc,
  dependencies: llama_deps,
  install: true
)