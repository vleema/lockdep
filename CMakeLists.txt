cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME lockdep)

project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_C_COMPILER /usr/bin/clang)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(COMPILE_OPTIONS
    "-Wall"
    "-Werror"
    "-Wextra"
    # "-fsanitize=address,undefined"
    "-fno-omit-frame-pointer"
    "-g3"
)

# set(LINK_OPTIONS "-fsanitize=address,undefined")

# Core lockdep library sources
file(GLOB LOCKDEP_SOURCES
    "src/lockdep/*.c"
)

# Library interposition sources
file(GLOB INTERPOSE_SOURCES
    "src/interpose/*.c"
    "src/lockdep/*.c"
)

# Test program sources
file(GLOB TEST_SOURCES "tests/*.c")

# Include directories
include_directories(src/include)

# Build the shared library for LD_PRELOAD
add_library(lockdep_interpose SHARED ${INTERPOSE_SOURCES})
target_compile_options(lockdep_interpose PRIVATE ${COMPILE_OPTIONS})
target_link_options(lockdep_interpose PRIVATE ${LINK_OPTIONS})
target_link_libraries(lockdep_interpose PRIVATE dl pthread)

# Build test programs
if(TEST_SOURCES)
    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_compile_options(${test_name} PRIVATE ${COMPILE_OPTIONS})
        target_link_options(${test_name} PRIVATE ${LINK_OPTIONS})
        target_link_libraries(${test_name} PRIVATE pthread)
    endforeach()
endif()
