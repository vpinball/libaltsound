#!/bin/bash

set -e

rm -rf external
mkdir external
cd external

#
# download bass24 framework and copy to platform/arch/frameworks
#

mkdir bass
cd bass
curl -s https://www.un4seen.com/files/bass24-ios.zip -o bass.zip
unzip bass.zip 
cp bass.h ../../third-party/include
lipo bass.xcframework/ios-arm64_armv7_armv7s/bass.framework/bass -extract arm64 -output libbass.dylib
install_name_tool -id @rpath/libbass.dylib libbass.dylib
codesign --force --sign - libbass.dylib
cp libbass.dylib ../../third-party/runtime-libs/ios/arm64/libbass.dylib
