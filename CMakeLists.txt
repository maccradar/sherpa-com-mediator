cmake_minimum_required(VERSION 2.8)

set(PROJECT_SOURCE_DIR pnp-reference)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_CXX_FLAGS "-Wall -Werror -fvisibility=hidden")

find_package(PkgConfig)
PKG_CHECK_MODULES(LUA REQUIRED lua5.1)
INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIRS})
LINK_DIRECTORIES(${LUA_LIBRARY_DIRS})

add_subdirectory(rfsmbinding)
add_subdirectory(app)
