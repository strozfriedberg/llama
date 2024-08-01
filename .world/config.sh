#!/bin/bash -ex

. .world/build_config.sh

# don't build 32-bit or 64-bit shared Windows for now
if [[ $Target == 'windows' && ($Architecture == '32' || $Linkage == 'shared') ]]; then
  exit
fi

if [ "$Target" != 'macos' ]; then
  CHECK_TARGET=check-valgrind
fi

if [ $Target = 'windows' ]; then
  # The test for setting DUCKDB_API in duckdb.h doesn't work correctly
  # when cross-compiling statically, so we set it here to ensure that
  # the compiler doesn't see all the duckdb functions declared with
  # __declspec(dllimport).
  CPPFLAGS+=' -DDUCKDB_API= '
  LDFLAGS+=' -fstack-protector'
fi

configure_it
