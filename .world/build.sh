#!/bin/bash -ex

. .world/build_config.sh

# don't build 32-bit or 64-bit shared Windows for now
if [[ $Target == 'windows' && ($Architecture == '32' || $Linkage == 'shared') ]]; then
  exit
fi

if [ "$Target" = 'windows' ]; then
  MAKE_FLAGS+=' LOG_COMPILER=.world/wine_wrapper.sh'
fi

make_it
make_check_it
