cmake_minimum_required(VERSION 3.20)

# set the project name
project(upnp-list-all)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")
set (CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)

# Build with dlna-server
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/dlna-server )
endif()

# build sketch as executable
set_source_files_properties(upnp-list-all.ino PROPERTIES LANGUAGE CXX)
add_executable (upnp-list-all upnp-list-all.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(upnp-list-all PUBLIC -DARDUINO -DIS_DESKTOP -DEXIT_ON_STOP -DPLATFORM_LINUX)

# specify libraries
target_link_libraries(upnp-list-all arduino_emulator dlna_server)