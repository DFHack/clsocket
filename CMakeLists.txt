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
        OPTION(CLSOCKET_OWN_INET_PTON "Use own inet_pton() implementation (required on MINGW)" ON)
        if(CLSOCKET_OWN_INET_PTON)
            add_definitions(-DCLSOCKET_OWN_INET_PTON)
        endif()
        # see https://cmake.org/pipermail/cmake/2012-September/051970.html
        # see http://stackoverflow.com/questions/13768515/how-to-do-static-linking-of-libwinpthread-1-dll-in-mingw
        # looks, the following compiler options produce linker errors. -static, .. options should only be used for linker
        #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static -static-libgcc")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
        # commenting out upper two CMAKE_C[XX]_FLAG now compiles and links with and without CLSOCKET_SHARED cmake option
        set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static -static-libgcc -s")
        set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static -static-libgcc -static-libstdc++ -s")
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

    if(MSVC)
        # Special MSVC stuff here
        set(CMAKE_C_FLAGS_RELEASE          "${CMAKE_C_FLAGS_RELEASE}          -MT")
        set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}        -MT")
        set(CMAKE_C_FLAGS_MINSIZEREL       "${CMAKE_C_FLAGS_MINSIZEREL}       -MT")
        set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL}     -MT")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO}   -MT")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -MT")
        set(CMAKE_C_FLAGS_DEBUG            "${CMAKE_C_FLAGS_DEBUG}            -MTd")
        set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}          -MTd")
    endif()
endif()
TARGET_LINK_LIBRARIES(clsocket ${PROJECT_LIBS})

# install into configured prefix
if(NOT CLSOCKET_DEP_ONLY)
    install(TARGETS clsocket ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
    install(FILES ${CLSOCKET_HEADERS} DESTINATION include)
else()

endif()

set_target_properties(clsocket PROPERTIES VERSION ${BUILD_VERSION}
                                          SOVERSION ${BUILD_MAJOR})

if(UNIX OR (WIN32 OR MINGW))
    OPTION(CLSOCKET_EXAMPLES "Build the examples" OFF)

    if(CLSOCKET_EXAMPLES)

        if (NOT MSVC)
            # pthread not available with MSVC
            ADD_EXECUTABLE(clsocket-example examples/RecvAsync.cpp)
            TARGET_LINK_LIBRARIES(clsocket-example clsocket pthread)
            if(NOT CLSOCKET_DEP_ONLY)
                install(TARGETS clsocket-example DESTINATION bin)
            endif()
        endif()

        ADD_EXECUTABLE(querydaytime-example examples/QueryDayTime.cpp)
        TARGET_LINK_LIBRARIES(querydaytime-example clsocket)

        ADD_EXECUTABLE(echoserver-example examples/EchoServer.cpp)
        TARGET_LINK_LIBRARIES(echoserver-example clsocket)

        ADD_EXECUTABLE(delayedechoserver-example examples/DelayedEchoServer.cpp)
        TARGET_LINK_LIBRARIES(delayedechoserver-example clsocket)

        ADD_EXECUTABLE(txtoserver-example examples/TxToServer.cpp)
        TARGET_LINK_LIBRARIES(txtoserver-example clsocket)

        ADD_EXECUTABLE(udpserver-example examples/UdpServer.cpp)
        TARGET_LINK_LIBRARIES(udpserver-example clsocket)

        ADD_EXECUTABLE(txtoudpserver-example examples/TxToUdpServer.cpp)
        TARGET_LINK_LIBRARIES(txtoudpserver-example clsocket)

    endif()
endif()

