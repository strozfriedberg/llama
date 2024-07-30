#!/bin/bash -ex

. .world/build_config.sh

# don't build 32-bit Windows for now
if [[ $Target == 'windows' && $Architecture == '32' ]]; then
  exit
fi

if [ "$Target" = 'windows' ]; then
  MAKE_FLAGS+=' LOG_COMPILER=.world/wine_wrapper.sh'
fi

make_it
make_check_it
