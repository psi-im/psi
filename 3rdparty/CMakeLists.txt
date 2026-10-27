cmake_minimum_required(VERSION 3.10.0)

# Native media playback also needs the range-capable loopback HTTP proxy,
# including Windows BASIC builds with Qt's FFmpeg multimedia backend.
include(qhttp.cmake)

# QSourceHighlite is pinned as the same upstream git submodule used in MatterLeast.
include(qsourcehighlite.cmake)
