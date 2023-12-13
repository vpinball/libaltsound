#!/bin/bash

set -e

rm -rf tmp
mkdir tmp
cd tmp

#
# download bass24 framework and copy to platform/arch/frameworks
#

curl -s https://www.un4seen.com/files/bass24-ios.zip -o bass.zip
unzip bass.zip 
cp bass.h ../../../../third-party/include
cp -r bass.xcframework/ios-arm64_armv7_armv7s/bass.framework ../../../../third-party/runtime-libs/tvos/arm64/frameworks

