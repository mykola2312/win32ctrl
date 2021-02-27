cmake_minimum_required(VERSION 3.18)
project(win32ctrl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    win32ctrl.cpp
    win32util.cpp
)

set(HEADERS
    win32ctrl.h
    win32util.h
)

add_library(win32ctrl SHARED ${SOURCES} ${HEADERS})
target_include_directories(win32ctrl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(win32ctrl PRIVATE
    comctl32
)
