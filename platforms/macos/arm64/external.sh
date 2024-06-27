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
lipo libbass.dylib -extract arm64 -output libbass-arm64.dylib
codesign --force --sign - libbass-arm64.dylib
cp bass.h ../../third-party/include
cp libbass-arm64.dylib ../../third-party/runtime-libs/macos/arm64/libbass.dylib
