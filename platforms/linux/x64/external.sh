#!/bin/bash

set -e

rm -rf tmp
mkdir tmp
cd tmp

#
# download bass24 and copy to platform/arch
#

curl -s https://www.un4seen.com/files/bass24-linux.zip -o bass.zip
unzip bass.zip
cp bass.h ../../../../third-party/include
cp libs/x86_64/libbass.so ../../../../third-party/runtime-libs/linux/x64

