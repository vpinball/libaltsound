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
curl -s https://www.un4seen.com/files/bass24.zip -o bass.zip
unzip bass.zip 
cp c/bass.h ../../third-party/include
cp c/x64/bass.lib ../../third-party/build-libs/win/x64
cp x64/bass.dll ../../third-party/runtime-libs/win/x64
