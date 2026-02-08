cmake_minimum_required(VERSION 4.2)
project(foobar_ai)

set(CMAKE_CXX_STANDARD 26)

add_library(foo_ai SHARED main.cpp
        Preferences.cpp
        Preferences.h
        resources.h
        foo_ai.rc
        mcp.cpp
        mcp.h
        initquit.cpp
        playlist_resource.cpp
        playlist_resource.h
        current_track_resource.cpp
        current_track_resource.h
)

find_package(foosdk REQUIRED)
find_package(mcp REQUIRED)
target_link_libraries(foo_ai PRIVATE foosdk::shared foosdk::foosdk foosdk::ppui foosdk::helpers mcp::mcp)
target_compile_definitions(foo_ai PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
