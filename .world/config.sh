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
  LDFLAGS+=' -fstack-protector'
fi

configure_it
