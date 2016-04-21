cmake_minimum_required(VERSION 2.8.6)
project (sherpa-communication-mediator)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(INSTALL_DIR /usr/lib CACHE PATH "Installation directory for mediator")


########################################################################
# LIBZMQ dependency
########################################################################
find_package(libzmq REQUIRED)
IF (LIBZMQ_FOUND)
    include_directories(${LIBZMQ_INCLUDE_DIRS})
    list(APPEND LIBS ${LIBZMQ_LIBRARIES})
ELSE (LIBZMQ_FOUND)
    message( FATAL_ERROR "libzmq not found." )
ENDIF (LIBZMQ_FOUND)

########################################################################
# CZMQ dependency
########################################################################
find_package(czmq REQUIRED)
IF (CZMQ_FOUND)
    include_directories(${CZMQ_INCLUDE_DIRS})
    list(APPEND LIBS ${CZMQ_LIBRARIES})
ELSE (CZMQ_FOUND)
    message( FATAL_ERROR "czmq not found." )
ENDIF (CZMQ_FOUND)

########################################################################
# Zyre dependency
########################################################################
find_package(zyre REQUIRED)
IF (ZYRE_FOUND)
    include_directories(${ZYRE_INCLUDE_DIRS})
    list(APPEND LIBS ${ZYRE_LIBRARIES})
ELSE (ZYRE_FOUND)
    message( FATAL_ERROR "czmq not found." )
ENDIF (ZYRE_FOUND)

########################################################################
# Jansson dependency
########################################################################
find_package(jansson REQUIRED)
IF (JANSSON_FOUND)
    include_directories(${JANSSON_INCLUDE_DIRS})
    list(APPEND LIBS ${JANSSON_LIBRARIES})
ELSE (JANSSON_FOUND)
    message( FATAL_ERROR "JANSSON not found." )
ENDIF (JANSSON_FOUND)

########################################################################
# Mediator
########################################################################
include_directories(${PROJECT_SOURCE_DIR}/include)
set(HEADER_FILES ${PROJECT_SOURCE_DIR}/include/mediator.h)

#add_library(sherpa_comm_mediator SHARED ${PROJECT_SOURCE_DIR}/src/sherpa_comm_mediator.c)
add_executable(sherpa_comm_mediator ${PROJECT_SOURCE_DIR}/src/sherpa_comm_mediator.c ${HEADER_FILES})
target_link_libraries(sherpa_comm_mediator ${LIBS})


install(TARGETS sherpa_comm_mediator DESTINATION ${INSTALL_DIR})

## Test executable ##
enable_testing()
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
set(CMAKE_CTEST_COMMAND ctest -V)

set_target_properties(check
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/test"
)
add_executable(mediator_selftest EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/test/mediator_selftest.c ${HEADER_FILES})
target_link_libraries(mediator_selftest ${LIBS})
add_test(mediator_selftest ${PROJECT_SOURCE_DIR}/bin/mediator_selftest ${PROJECT_SOURCE_DIR}/examples/configs/donkey.json)
add_dependencies(check mediator_selftest)

# gcc -ggdb $< -lczmq -lzmq - -ljansson -o $@
