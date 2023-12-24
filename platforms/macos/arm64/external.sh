#!/bin/bash

set -e

rm -rf external
mkdir external
cd external

#
# download bass24 and copy to platform/arch
#

mkdir bass
cd bass
curl -s https://www.un4seen.com/files/bass24-osx.zip -o bass.zip
unzip bass.zip 
cp bass.h ../../third-party/include
cp libbass.dylib ../../third-party/runtime-libs/macos/arm64
