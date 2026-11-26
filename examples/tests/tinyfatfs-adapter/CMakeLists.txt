cmake_minimum_required(VERSION 3.20)

# set the project name
project(tinyfatfs-adapter)
set (CMAKE_CXX_STANDARD 17)

include(FetchContent)

# Build with dlna-server
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/dlna-server )
endif()

# TinyFATFS is header only: we only need the sources (its own CMakeLists.txt
# would enable its examples and tests); use -DFETCHCONTENT_SOURCE_DIR_TINYFATFS=<dir>
# to use a local copy
FetchContent_Declare(tinyfatfs GIT_REPOSITORY "https://github.com/pschatzmann/TinyFATFS.git" GIT_TAG main )
FetchContent_GetProperties(tinyfatfs)
if(NOT tinyfatfs_POPULATED)
    FetchContent_Populate(tinyfatfs)
endif()

# build sketch as executable
set_source_files_properties(tinyfatfs-adapter.ino PROPERTIES LANGUAGE CXX)
add_executable (tinyfatfs-adapter tinyfatfs-adapter.ino)
target_include_directories(tinyfatfs-adapter PRIVATE ${tinyfatfs_SOURCE_DIR}/src)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(tinyfatfs-adapter PUBLIC -DARDUINO -DIS_DESKTOP -DEXIT_ON_STOP)

# specify libraries
target_link_libraries(tinyfatfs-adapter arduino_emulator arduino-dlna)
