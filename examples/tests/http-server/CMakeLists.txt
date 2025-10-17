cmake_minimum_required(VERSION 3.20)

# set the project name
project(http-server)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

include(FetchContent)

# Build with dlna-server when invoked standalone
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/dlna-server )
endif()

# build sketch as executable
set_source_files_properties(http-server.ino PROPERTIES LANGUAGE CXX)
add_executable (http-server http-server.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(http-server PUBLIC -DARDUINO -DIS_DESKTOP -DEXIT_ON_STOP)

# specify libraries
target_link_libraries(http-server arduino_emulator arduino-dlna)
