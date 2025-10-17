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


file(GLOB_RECURSE SRC_LIST_CPP CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/*.cpp" )

# define libraries
add_library (arduino-dlna ${SRC_LIST_CPP})

# prevent compile errors
target_compile_options(arduino-dlna PUBLIC -DNO_GATEWAYIP -DIS_DESKTOP )

# define location for header files
target_include_directories(arduino-dlna PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/Basic ${CMAKE_SOURCE_DIR}/src/DLNA ${CMAKE_SOURCE_DIR}/src/Platform ${CMAKE_SOURCE_DIR}/src/TinyHttp  ${CMAKE_SOURCE_DIR}/src/DLNA )

# specify libraries
target_link_libraries(arduino-dlna arduino_emulator)

if(DLNA_BUILD_EXAMPLES)
    add_subdirectory("examples")
endif()
