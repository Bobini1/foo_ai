cmake_minimum_required(VERSION 4.2)
project(foobar_ai)

set(CMAKE_CXX_STANDARD 26)

add_library(foo_ai SHARED main.cpp
        Preferences.cpp
        Preferences.h
        guid.h
        resources.h
        foo_ai.rc
)

## Add code to download https://www.foobar2000.org/downloads/SDK-2024-12-03.7z, unpack it to foobar_sdk and create a target foobar_sdk
find_package(foosdk REQUIRED)
find_package(mcp REQUIRED)
target_link_libraries(foo_ai PRIVATE foosdk::shared foosdk::foosdk foosdk::ppui foosdk::helpers mcp::mcp)
