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
lipo libbass.dylib -extract x86_64 -output libbass-x64.dylib
codesign --force --sign - libbass-x64.dylib
cp bass.h ../../third-party/include
cp libbass-x64.dylib ../../third-party/runtime-libs/macos/x64/libbass.dylib
