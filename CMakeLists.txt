cmake_minimum_required(VERSION 3.21)
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
    add_definitions(-DWIN32)
    SET(PROJECT_LIBS Ws2_32.lib)
    if(MINGW)
        # Special MINGW stuff here
    elseif(MSVC)
        # Special MSVC stuff here
        add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
    else()
        # No idea what it is, but there's a good chance it's too weird.
        MESSAGE( FATAL_ERROR "Using unknown WIN32 compiler... NOT. Please add to build system." )
    endif()
endif()

OPTION(CLSOCKET_SHARED "Build clsocket lib as shared." ON)
OPTION(CLSOCKET_DEP_ONLY "Build for use inside other CMake projects as dependency." OFF)

# make the lib
if(CLSOCKET_SHARED)
    if(CLSOCKET_DEP_ONLY)
        ADD_LIBRARY(clsocket SHARED EXCLUDE_FROM_ALL ${CLSOCKET_SOURCES})
    else()
        ADD_LIBRARY(clsocket SHARED ${CLSOCKET_SOURCES})
    endif()
else()
    if(CLSOCKET_DEP_ONLY)
        ADD_LIBRARY(clsocket STATIC EXCLUDE_FROM_ALL ${CLSOCKET_SOURCES})
    else()
        ADD_LIBRARY(clsocket STATIC ${CLSOCKET_SOURCES})
    endif()
endif()
TARGET_LINK_LIBRARIES(clsocket ${PROJECT_LIBS})
TARGET_INCLUDE_DIRECTORIES(clsocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# install into configured prefix
if(NOT CLSOCKET_DEP_ONLY)
    install(TARGETS clsocket ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
    install(FILES ${CLSOCKET_HEADERS} DESTINATION include)
else()

endif()

set_target_properties(clsocket PROPERTIES VERSION ${BUILD_VERSION}
                                          SOVERSION ${BUILD_MAJOR})

if(UNIX)
    OPTION(CLSOCKET_EXAMPLES "Build the examples" OFF)

    if(CLSOCKET_EXAMPLES)
        ADD_EXECUTABLE(clsocket-example examples/RecvAsync.cpp)
        TARGET_LINK_LIBRARIES(clsocket-example clsocket pthread)
        if(NOT CLSOCKET_DEP_ONLY)
            install(TARGETS clsocket-example DESTINATION bin)
        endif()

        ADD_EXECUTABLE(querydaytime-example examples/QueryDayTime.cpp)
        TARGET_LINK_LIBRARIES(querydaytime-example clsocket)

        ADD_EXECUTABLE(echoserver-example examples/EchoServer.cpp)
        TARGET_LINK_LIBRARIES(echoserver-example clsocket)
    endif()
endif()

