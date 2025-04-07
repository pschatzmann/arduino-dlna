cmake_minimum_required(VERSION 3.16)

# set the project name
project(dlna_server)

# lots of warnings and all warnings as errors
## add_compile_options(-Wall -Wextra )
set(CMAKE_CXX_STANDARD 17)
set (CMAKE_INCLUDE_CURRENT_DIR ON)

include(FetchContent)

FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(arduino_emulator)
if(NOT arduino_emulator_POPULATED)
    FetchContent_Populate(arduino_emulator)
    add_subdirectory(${arduino_emulator_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/emulator)
endif()

# Add AudioTools 
add_compile_options(-DIS_DESKTOP)
FetchContent_Declare(arduino-audio-tools GIT_REPOSITORY "https://github.com/pschatzmann/arduino-audio-tools.git" GIT_TAG "main" )
FetchContent_GetProperties(arduino-audio-tools)
if(NOT arduino-audio-tools_POPULATED)
    FetchContent_Populate(arduino-audio-tools)
    add_subdirectory(${arduino-audio-tools_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools)
endif()


file(GLOB_RECURSE SRC_LIST_CPP CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/*.cpp" )

# define libraries
add_library (dlna_server ${SRC_LIST_CPP})

# prevent compile errors
target_compile_options(dlna_server PUBLIC -DNO_GATEWAYIP -DIS_DESKTOP )

# define location for header files
target_include_directories(dlna_server PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/Basic ${CMAKE_SOURCE_DIR}/src/DLNA ${CMAKE_SOURCE_DIR}/src/Platform ${CMAKE_SOURCE_DIR}/src/TinyHttp  ${CMAKE_SOURCE_DIR}/src/DLNA )

# specify libraries
target_link_libraries(dlna_server arduino_emulator arduino-audio-tools)

# build examples
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/device-xml")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/device-xml-parser")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/udp-send")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/udp-receive")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/udp-receive1")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/device-media-renderer")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/control-point-light")
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/control-point-light-fast")
