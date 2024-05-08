cmake_minimum_required(VERSION 3.16)

# set the project name
project(dlna_server)

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

file(GLOB_RECURSE SRC_LIST_CPP CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/*.cpp" )

# define libraries
add_library (dlna_server ${SRC_LIST_CPP})

# prevent compile errors
target_compile_options(dlna_server PUBLIC -DNO_GATEWAYIP -DIS_DESKTOP )

# define location for header files
target_include_directories(dlna_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/Basic ${CMAKE_CURRENT_SOURCE_DIR}/src/DLNA ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform ${CMAKE_CURRENT_SOURCE_DIR}/src/TinyHttp ${CMAKE_CURRENT_SOURCE_DIR}/src/UPnP ${CMAKE_CURRENT_SOURCE_DIR}/src/DLNA )

# specify libraries
target_link_libraries(dlna_server arduino_emulator)

# build examples
add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/device-xml")
#add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/upnp-list-all")
#add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/examples/tests/upnp-update")
