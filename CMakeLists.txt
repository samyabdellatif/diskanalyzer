cmake_minimum_required(VERSION 3.15)
project(DiskAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCE_FILES
    src/main.cpp
    src/disk_manager.cpp
    src/disk_test.cpp
    src/ui.cpp
    src/resource.rc
)

add_executable(DiskAnalyzer WIN32 ${SOURCE_FILES})

target_include_directories(DiskAnalyzer PRIVATE src)
target_compile_definitions(DiskAnalyzer PRIVATE UNICODE _UNICODE)
target_link_libraries(DiskAnalyzer PRIVATE comctl32 setupapi)
if (MSVC)
    target_link_options(DiskAnalyzer PRIVATE "/MANIFEST:NO")
endif()
