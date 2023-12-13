#------------------------------------------------------------------------------
# altsound library
# 07/04/2023 Dave Roscoe
#
# CMake project to build altsound code as a standalone static library.
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.25)

set(CMAKE_SYSTEM_NAME iOS)

option(BUILD_SHARED "Option to build shared library" ON)
option(BUILD_STATIC "Option to build static library" ON)

project(altsound)

set(CMAKE_OSX_DEPLOYMENT_TARGET 16.0)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 99)

add_compile_definitions(
)

add_definitions( "-DINLINE=static inline __attribute__((always_inline))" )

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
)

set(ALTSOUND_INCLUDE_DIRS
   src
   third-party/include
)

set(ALTSOUND_FRAMEWORKS_DIR
   ${CMAKE_SOURCE_DIR}/third-party/runtime-libs/ios/arm64/frameworks
)

if(BUILD_SHARED)
   add_library(altsound_shared SHARED ${ALTSOUND_SOURCES})
   target_include_directories(altsound_shared PUBLIC ${ALTSOUND_INCLUDE_DIRS})
   target_link_options(altsound_shared PUBLIC "-Wl,-F${ALTSOUND_FRAMEWORKS_DIR}")
   target_link_libraries(altsound_shared PUBLIC
      "-framework bass"
   )
   set_target_properties(altsound_shared PROPERTIES
      OUTPUT_NAME "altsound"
   )
endif()

if(BUILD_STATIC)
   add_library(altsound_static STATIC ${ALTSOUND_SOURCES})
   target_include_directories(altsound_static PUBLIC ${ALTSOUND_INCLUDE_DIRS})
   set_target_properties(altsound_static PROPERTIES
      OUTPUT_NAME "altsound"
   )
endif()
