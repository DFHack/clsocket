cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(clsocket)

# set up versioning.
set(BUILD_MAJOR "1")
set(BUILD_MINOR "4")
set(BUILD_VERSION "3")
set(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_VERSION})

SET(CLSOCKET_HEADERS
src/ActiveSocket.h
src/Host.h
src/PassiveSocket.h
src/SimpleSocket.h
src/StatTimer.h
)

SET(CLSOCKET_SOURCES
src/SimpleSocket.cpp
src/ActiveSocket.cpp
src/PassiveSocket.cpp
)

# OS and compiler checks.
if(WIN32)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    SET(PROJECT_LIBS Ws2_32.lib)
endif()

option(BUILD_SHARED_LIBS "Build libs as shared" ON)
if(DEFINED CLSOCKET_SHARED)
    message(FATAL_ERROR "The CLSOCKET_SHARED option is deprecated, instead use BUILD_SHARED_LIBS.")
endif()
if(DEFINED CLSOCKET_DEP_ONLY)
    message(FATAL_ERROR "The CLSOCKET_DEP_ONLY option is deprecated, instead use add_subdirectory() with EXCLUDE_FROM_ALL.")
endif()

# make the lib
# add headers so that they appear in IDEs
add_library(clsocket ${CLSOCKET_SOURCES} ${CLSOCKET_HEADERS})
TARGET_LINK_LIBRARIES(clsocket ${PROJECT_LIBS})
# target_include_directories was added in 2.8.11
set_target_properties(clsocket PROPERTIES
    INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/src
    INTERFACE_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/src
)

# install into configured prefix
install(TARGETS clsocket DESTINATION lib)
install(FILES ${CLSOCKET_HEADERS} DESTINATION include)

set_target_properties(clsocket PROPERTIES VERSION ${BUILD_VERSION}
                                          SOVERSION ${BUILD_MAJOR})

if(UNIX)
    OPTION(CLSOCKET_EXAMPLES "Build the examples" OFF)

    if(CLSOCKET_EXAMPLES)
        ADD_EXECUTABLE(clsocket-example examples/RecvAsync.cpp)
        TARGET_LINK_LIBRARIES(clsocket-example clsocket pthread)
        install(TARGETS clsocket-example DESTINATION bin)

        ADD_EXECUTABLE(querydaytime-example examples/QueryDayTime.cpp)
        TARGET_LINK_LIBRARIES(querydaytime-example clsocket)

        ADD_EXECUTABLE(echoserver-example examples/EchoServer.cpp)
        TARGET_LINK_LIBRARIES(echoserver-example clsocket)
    endif()
endif()

