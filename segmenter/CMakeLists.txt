cmake_minimum_required(VERSION 3.16)
project(mini_hls_segmenter CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)

add_executable(segmenter src/main.cpp)
target_link_libraries(segmenter PRIVATE PkgConfig::FFMPEG)
target_compile_definitions(segmenter PRIVATE __STDC_CONSTANT_MACROS)
