#!/bin/bash

#some of buildtype are: debug, release: 
if [[ ! -z "$1" ]]; then
  meson build  --buildtype=$1 --wipe
fi

ninja -C build

