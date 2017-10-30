cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(clsocket)

# set up versioning.
set(BUILD_MAJOR "1")
set(BUILD_MINOR "4")
set(BUILD_VERSION "3")
set(BUILD_VERSION ${BUILD_MAJOR}.${BUILD_MINOR}.${BUILD_VERSION})

include_directories(src)

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

# mark headers as headers...
SET_SOURCE_FILES_PROPERTIES( ${CLSOCKET_HEADERS} PROPERTIES HEADER_FILE_ONLY TRUE )
# append to sources so that dependency checks work on headers
LIST(APPEND CLSOCKET_SOURCES ${CLSOCKET_HEADERS})

# OS and compiler checks. 
if(UNIX)
    # linux / normal unix
    add_definitions(-D_LINUX)
    if(CYGWIN)
        # Special Cygwin stuff here
    elseif(APPLE)
        # Special Apple stuff here
        remove_definitions(-D_LINUX)
        add_definitions(-D_DARWIN)
    endif()
elseif(WIN32)
    add_definitions(-DWIN32 -D_WINSOCK_DEPRECATED_NO_WARNINGS)
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
add_library(clsocket ${CLSOCKET_SOURCES})
TARGET_LINK_LIBRARIES(clsocket ${PROJECT_LIBS})

# install into configured prefix
install(TARGETS clsocket ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
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

