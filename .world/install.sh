#!/bin/bash -ex

. .world/build_config.sh

# don't build 32-bit Windows for now
if [[ $Target == 'windows' && $Architecture == '32' ]]; then
  exit
fi

install_it
