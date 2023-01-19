#!/bin/bash -ex

. .world/build_config.sh

if [ "$Target" != 'macos' ]; then
  CHECK_TARGET=check-valgrind
fi

if [ $Target = 'windows' ]; then
  LDFLAGS+=' -fstack-protector'
fi

configure_it
