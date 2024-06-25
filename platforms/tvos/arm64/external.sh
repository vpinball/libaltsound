#!/bin/bash

set -e

rm -rf external
mkdir external
cd external

#
# download bass24 framework and copy to platform/arch/frameworks
#
# Note: The BASS library needs to be replaced with the tvOS version.
# Leaving the iOS version here so we can test the compile for static builds. Shared builds will fail when linking. 
#

mkdir bass
cd bass
curl -s https://www.un4seen.com/files/bass24-ios.zip -o bass.zip
unzip bass.zip 
cp bass.h ../../third-party/include
cp -r bass.xcframework/ios-arm64_armv7_armv7s/bass.framework ../../third-party/runtime-libs/tvos/arm64/frameworks

