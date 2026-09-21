cmake_minimum_required(VERSION 3.15)
project(DefThumbnailProvider VERSION 1.0.0 LANGUAGES CXX)
if(NOT WIN32)
  message(FATAL_ERROR "Windows-only project")
endif()
option(DEFTHUMB_ENABLE_DEBUG_LOG "Enable OutputDebugString diagnostics" OFF)
add_library(defdecoder STATIC src/DefDecoder.cpp src/DefDecoder.h)
target_compile_features(defdecoder PUBLIC cxx_std_17)
target_include_directories(defdecoder PUBLIC src)
add_library(DefThumbnailProvider SHARED
  src/ClassFactory.cpp src/ClassFactory.h src/ComHelpers.h src/DllGlobals.cpp src/DllGlobals.h
  src/ImageScaler.cpp src/ImageScaler.h src/Registration.cpp src/Registration.h
  src/ThumbnailProvider.cpp src/ThumbnailProvider.h src/dllmain.cpp src/DefThumbnailProvider.exports.def)
target_compile_features(DefThumbnailProvider PRIVATE cxx_std_17)
target_compile_definitions(DefThumbnailProvider PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  WINVER=0x0501
  _WIN32_WINNT=0x0501
  NTDDI_VERSION=0x05010300)
if(DEFTHUMB_ENABLE_DEBUG_LOG)
  target_compile_definitions(DefThumbnailProvider PRIVATE DEFTHUMB_ENABLE_DEBUG_LOG=1)
endif()
target_link_libraries(DefThumbnailProvider PRIVATE defdecoder shlwapi ole32 shell32)
if(MSVC AND CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
  target_link_options(DefThumbnailProvider PRIVATE "/SUBSYSTEM:WINDOWS,5.01")
elseif(MSVC AND CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
  target_link_options(DefThumbnailProvider PRIVATE "/SUBSYSTEM:WINDOWS,5.02")
endif()
add_executable(DefDump tools/DefDump.cpp)
target_compile_features(DefDump PRIVATE cxx_std_17)
target_compile_definitions(DefDump PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(DefDump PRIVATE defdecoder)
add_executable(DefDecoderTests tests/DefDecoderTests.cpp)
target_compile_features(DefDecoderTests PRIVATE cxx_std_17)
target_link_libraries(DefDecoderTests PRIVATE defdecoder)
enable_testing()
add_test(NAME DefDecoderTests COMMAND DefDecoderTests)
add_executable(ShellSmoke tools/ShellSmoke.cpp)
target_compile_features(ShellSmoke PRIVATE cxx_std_17)
target_compile_definitions(ShellSmoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ShellSmoke PRIVATE ole32 shell32 gdi32)
add_executable(ComSmoke tools/ComSmoke.cpp)
target_compile_features(ComSmoke PRIVATE cxx_std_17)
target_compile_definitions(ComSmoke PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ComSmoke PRIVATE ole32 shlwapi gdi32)
if(MSVC)
  foreach(t defdecoder DefThumbnailProvider DefDump DefDecoderTests ShellSmoke ComSmoke)
    target_compile_options(${t} PRIVATE /W4 /permissive- /EHsc /utf-8)
    set_property(TARGET ${t} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endforeach()
endif()
