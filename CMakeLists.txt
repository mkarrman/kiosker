cmake_minimum_required(VERSION 3.10)

project(kiosker C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/src/cmake/")

find_package(PkgConfig REQUIRED) 

add_subdirectory(src)

