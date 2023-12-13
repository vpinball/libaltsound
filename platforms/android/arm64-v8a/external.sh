#!/bin/bash

set -e

rm -rf tmp
mkdir tmp
cd tmp

#
# download bass24 and copy to platform/arch
#

curl -sL https://www.un4seen.com/files/bass24-android.zip -o bass.zip
unzip bass.zip
cp c/bass.h ../../../../third-party/include
cp libs/arm64-v8a/libbass.so ../../../../third-party/runtime-libs/android/arm64-v8a
