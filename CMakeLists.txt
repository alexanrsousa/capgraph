cmake_minimum_required(VERSION 3.10)
project(CapGraph VERSION 0.1)

#set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} comctrl32.lib")
#set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} comctrl32.lib")
#MESSAGE(STATUS "CMAKE_CXX_STANDARD_LIBRARIES= ${CMAKE_CXX_STANDARD_LIBRARIES}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCE_FILES
    application.manifest
    resources.rc
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/rectwindow.cpp
    src/rectwindow.h
    src/window.h
)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
add_executable(${CMAKE_PROJECT_NAME} WIN32 ${SOURCE_FILES})
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC comctl32.lib)
