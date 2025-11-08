cmake_minimum_required(VERSION 3.16)

# set the project name
project(arduino-dlna)

# Option to enable/disable building examples (default: ON)
option(DLNA_BUILD_EXAMPLES "Build example sketches" ON)

# lots of warnings and all warnings as errors
## add_compile_options(-Wall -Wextra )
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(arduino_emulator GIT_REPOSITORY "https://github.com/pschatzmann/Arduino-Emulator.git" GIT_TAG main )
FetchContent_GetProperties(arduino_emulator)
if(NOT arduino_emulator_POPULATED)
    FetchContent_Populate(arduino_emulator)
    add_subdirectory(${arduino_emulator_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/emulator)
endif()

add_compile_options(-DIS_DESKTOP)

# Header-only core library: expose include paths & compile definitions only.
# We use an INTERFACE target so consumers can link against it while
# inheriting the required include directories and defines.
add_library(arduino-dlna INTERFACE)

# prevent compile errors
target_compile_definitions(arduino-dlna INTERFACE NO_GATEWAYIP IS_DESKTOP)
target_compile_options(arduino-dlna INTERFACE)

# define location for header files
target_include_directories(arduino-dlna INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/basic>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/dlna>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/http>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/udp>
    $<INSTALL_INTERFACE:include>
)

# specify libraries
target_link_libraries(arduino-dlna INTERFACE arduino_emulator)

if(DLNA_BUILD_EXAMPLES)
    add_subdirectory("examples")
endif()
