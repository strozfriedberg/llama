project('llama', 'cpp',
  version: '0.0.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=false'
  ]
)

# Additional compiler flags beyond warning_level=3
add_project_arguments([
  '-Wnon-virtual-dtor', '-pedantic', '-pipe'
], language: 'cpp')

# Debug/Release flags
if get_option('buildtype') == 'debug'
  add_project_arguments('-Og', language: 'cpp')
else
  add_project_arguments('-O3', '-march=native', language: 'cpp')
endif

# Threading support
threads_dep = dependency('threads')

# Boost dependencies
boost_dep = dependency('boost', modules: ['date_time', 'program_options', 'system'])

# Additional system libraries for Windows
if host_machine.system() == 'windows'
  ws2_32_dep = meson.get_compiler('cpp').find_library('ws2_32', required: true)
  mswsock_dep = meson.get_compiler('cpp').find_library('mswsock', required: true)
  boost_system_deps = [boost_dep, ws2_32_dep, mswsock_dep]
  add_project_arguments('-DWIN32_LEAN_AND_MEAN', language: 'cpp')
else
  boost_system_deps = [boost_dep]
endif

# pkg-config dependencies
tsk_dep = dependency('tsk', required: true)
libarchive_dep = dependency('libarchive', required: true) 
hasher_dep = dependency('hasher', required: true)
lightgrep_dep = dependency('lightgrep', required: true)
yara_dep = dependency('yara', required: false)

# Header-only dependencies
jsoncons_dep = meson.get_compiler('cpp').find_library('', required: false)
if not meson.get_compiler('cpp').has_header('jsoncons/json.hpp')
  error('jsoncons/json.hpp header not found')
endif

# DuckDB dependency
duckdb_dep = meson.get_compiler('cpp').find_library('duckdb', required: true)
if not meson.get_compiler('cpp').has_header('duckdb.h')
  error('duckdb.h header not found')
endif

# Optional Address Sanitizer support
asan_option = get_option('asan')
if asan_option
  add_project_arguments('-fsanitize=address', '-fno-omit-frame-pointer', language: 'cpp')
  add_project_link_arguments('-fsanitize=address', language: 'cpp')
endif

# Catch2 for testing (optional)
catch2_dep = dependency('catch2-with-main', required: false)

# Collect all main dependencies
llama_deps = [
  threads_dep,
  boost_system_deps,
  tsk_dep,
  libarchive_dep,
  hasher_dep,
  lightgrep_dep,
  duckdb_dep
]

if yara_dep.found()
  llama_deps += yara_dep
  add_project_arguments('-DHAS_YARA', language: 'cpp')
endif

# Include directories
inc = include_directories('include')

# Build subdirectories
subdir('src')

if catch2_dep.found()
  subdir('test')
endif
