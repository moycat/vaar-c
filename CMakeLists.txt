cmake_minimum_required(VERSION 3.16)
project(vaar C)

set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -xHost")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable(vaar src/main.c src/buf_pool.c src/buf_pool.h src/dir_entry.c src/dir_entry.h src/format.h src/archive.c src/archive.h src/path.h src/writer.c src/writer.h)
add_definitions(-D_GNU_SOURCE)
target_link_libraries(vaar pthread uring)
target_link_libraries(vaar -static)
