cmake_minimum_required(VERSION 3.25)

set(PLATFORM "win" CACHE STRING "Platform")
set(ARCH "x64" CACHE STRING "Arch")

option(BUILD_SHARED "Option to build shared library" ON)
option(BUILD_STATIC "Option to build static library" ON)

message(STATUS "PLATFORM: ${PLATFORM}")
message(STATUS "ARCH: ${ARCH}")

message(STATUS "BUILD_SHARED: ${BUILD_SHARED}")
message(STATUS "BUILD_STATIC: ${BUILD_STATIC}")

if(PLATFORM STREQUAL "macos")
   set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
elseif(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "ios-simulator")
   set(CMAKE_SYSTEM_NAME iOS)
   if (PLATFORM STREQUAL "ios-simulator")
      set(CMAKE_OSX_SYSROOT iphonesimulator)
   endif()
   set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0)
elseif(PLATFORM STREQUAL "tvos")
   set(CMAKE_SYSTEM_NAME tvOS)
   set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0)
elseif(PLATFORM STREQUAL "android")
   set(CMAKE_SYSTEM_NAME Android)
   set(CMAKE_SYSTEM_VERSION 30)
   set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
endif()

file(READ src/altsound.h version)
string(REGEX MATCH "ALTSOUND_VERSION_MAJOR[ ]+([0-9]+)" _tmp ${version})
set(VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "ALTSOUND_VERSION_MINOR[ ]+([0-9]+)" _tmp ${version})
set(VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "ALTSOUND_VERSION_PATCH[ ]+([0-9]+)" _tmp ${version})
set(VERSION_PATCH "${CMAKE_MATCH_1}")

project(altsound VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
	DESCRIPTION "Cross-platform altsound library for pinmame")

if(PLATFORM STREQUAL "macos")
   if (ARCH STREQUAL "arm64")
      set(CMAKE_OSX_ARCHITECTURES arm64)
   elseif(ARCH STREQUAL "x64")
      set(CMAKE_OSX_ARCHITECTURES x86_64)
   endif()
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "@executable_path")
elseif(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "ios-simulator" OR PLATFORM STREQUAL "tvos")
   set(CMAKE_OSX_ARCHITECTURES arm64)
elseif(PLATFORM STREQUAL "linux" OR PLATFORM STREQUAL "android")
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(ALTSOUND_SOURCES
   src/altsound_data.cpp
   src/altsound_data.hpp
   src/gsound_csv_parser.cpp
   src/gsound_csv_parser.hpp
   src/altsound_ini_processor.hpp
   src/altsound_ini_processor.cpp
   src/altsound_logger.cpp
   src/altsound_logger.hpp
   src/altsound_processor_base.cpp
   src/altsound_processor_base.hpp
   src/altsound_processor.cpp
   src/altsound_processor.hpp
   src/altsound_file_parser.cpp
   src/altsound_file_parser.hpp
   src/altsound_csv_parser.cpp
   src/altsound_csv_parser.hpp
   src/gsound_processor.cpp
   src/gsound_processor.hpp
   src/altsound.cpp
   src/altsound.h
   src/miniaudio_private.c
   src/miniaudio_private.h
   src/miniaudio_bass_compat.cpp
   src/miniaudio_bass_compat.hpp
)

if(PLATFORM STREQUAL "win")
   list(APPEND ALTSOUND_SOURCES
      third-party/include/dirent/dirent.c
      third-party/include/dirent/dirent.h
   )
endif()

set(ALTSOUND_INCLUDE_DIRS
   src
   third-party/include
)

if(BUILD_SHARED)
   add_library(altsound_shared SHARED ${ALTSOUND_SOURCES})
   target_include_directories(altsound_shared PUBLIC ${ALTSOUND_INCLUDE_DIRS})

   if(PLATFORM STREQUAL "win" AND ARCH STREQUAL "x64")
      set(ALTSOUND_OUTPUT_NAME "altsound64")
   else()
      set(ALTSOUND_OUTPUT_NAME "altsound")
   endif()

   set_target_properties(altsound_shared PROPERTIES
      OUTPUT_NAME ${ALTSOUND_OUTPUT_NAME}
      VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
   )

   install(TARGETS altsound_shared
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
   install(FILES src/altsound.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux")
      add_executable(altsound_test
         src/test.cpp
      )

      target_link_libraries(altsound_test PUBLIC altsound_shared)
   endif()
endif()

if(BUILD_STATIC)
   add_library(altsound_static STATIC ${ALTSOUND_SOURCES})

   target_include_directories(altsound_static PUBLIC ${ALTSOUND_INCLUDE_DIRS})

   if(PLATFORM STREQUAL "win")
      set_target_properties(altsound_static PROPERTIES
         OUTPUT_NAME "altsound_static"
      )
   else()
      set_target_properties(altsound_static PROPERTIES
         OUTPUT_NAME "altsound"
      )
   endif()

   install(TARGETS altsound_static
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
   install(FILES src/altsound.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux")
      add_executable(altsound_test_s
         src/test.cpp
      )

      target_link_libraries(altsound_test_s PUBLIC altsound_static)
   endif()
endif()
