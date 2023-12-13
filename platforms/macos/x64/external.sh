#!/bin/bash

set -e

rm -rf tmp
mkdir tmp
cd tmp

#
# download bass24 and copy to platform/arch
#

curl -s https://www.un4seen.com/files/bass24-osx.zip -o bass.zip
unzip bass.zip 
cp bass.h ../../../../third-party/include
cp libbass.dylib ../../../../third-party/runtime-libs/macos/x64
