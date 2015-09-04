#!/bin/bash

# clone and build wlc
git clone https://github.com/Cloudef/wlc.git
cd wlc
git submodule update --init --recursive # - initialize and fetch submodules
mkdir target && cd target               # - create build target directory
cmake -DCMAKE_BUILD_TYPE=Upstream ..    # - run CMake
make                                    # - compile

cd ../..

# build sway
cmake \
    -DWLC_LIBRARIES=wlc/target/src/libwlc.so \
    -DWLC_INCLUDE_DIRS=wlc/include .
make
