cmake_minimum_required(VERSION 3.20)

# set the project name
project(xml-parser-action)
set (CMAKE_CXX_STANDARD 11)

include(FetchContent)

# Build with dlna-server when the example is built standalone
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/dlna-server )
endif()

# build sketch as executable
set_source_files_properties(xml-parser-action.ino PROPERTIES LANGUAGE CXX)
add_executable (xml-parser-action xml-parser-action.ino)

# set preprocessor defines
target_compile_definitions(arduino_emulator PUBLIC -DDEFINE_MAIN)
target_compile_definitions(xml-parser-action PUBLIC -DARDUINO -DIS_DESKTOP -DEXIT_ON_STOP)

# specify libraries
target_link_libraries(xml-parser-action arduino_emulator arduino-dlna)
