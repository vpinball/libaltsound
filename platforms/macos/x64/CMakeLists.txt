#------------------------------------------------------------------------------
# altsound library
# 07/04/2023 Dave Roscoe
#
# CMake project to build altsound code as a standalone static library.
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.25)

option(BUILD_SHARED "Option to build shared library" ON)
option(BUILD_STATIC "Option to build static library" ON)

project(altsound)

set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
set(CMAKE_OSX_ARCHITECTURES x86_64)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "@executable_path")

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

set(ALTSOUND_LIB_DIRS
   third-party/runtime-libs/macos/x64
)

set(ALTSOUND_LIBS
   bass
)

if(BUILD_SHARED)
   add_library(altsound_shared SHARED ${ALTSOUND_SOURCES})
   target_include_directories(altsound_shared PUBLIC ${ALTSOUND_INCLUDE_DIRS})
   target_link_directories(altsound_shared PUBLIC ${ALTSOUND_LIB_DIRS})
   set_target_properties(altsound_shared PROPERTIES
      OUTPUT_NAME "altsound"
   )
   target_link_libraries(altsound_shared ${ALTSOUND_LIBS})

   add_executable(altsound_test
      src/test.cpp
   )
   
   target_link_libraries(altsound_test altsound_shared)

   add_custom_command(TARGET altsound_test POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/macos/x64/libbass.dylib" "$<TARGET_FILE_DIR:altsound_test>"
   )
endif()

if(BUILD_STATIC)
   add_library(altsound_static STATIC ${ALTSOUND_SOURCES})
   target_include_directories(altsound_static PUBLIC ${ALTSOUND_INCLUDE_DIRS})
   set_target_properties(altsound_static PROPERTIES
      OUTPUT_NAME "altsound"
   )
 
   add_executable(altsound_test_s
      src/test.cpp
   )
    
   target_link_directories(altsound_test_s PUBLIC ${ALTSOUND_LIB_DIRS})
   target_link_libraries(altsound_test_s altsound_static ${ALTSOUND_LIBS})

   add_custom_command(TARGET altsound_test_s POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/macos/x64/libbass.dylib" "$<TARGET_FILE_DIR:altsound_test_s>"
   )
endif()
 
