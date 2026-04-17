cmake_minimum_required(VERSION 3.21)
project(reactive-synthesis-bfss)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable RPATH for local libraries
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_RPATH "${PROJECT_SOURCE_DIR}/libs/spot/inst/lib;${PROJECT_SOURCE_DIR}/libs/abc")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Detect Git Root and Main Repository (for Worktree fallback)
execute_process(COMMAND git rev-parse --path-format=absolute --git-common-dir
                OUTPUT_VARIABLE GIT_COMMON_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)

if(GIT_COMMON_DIR)
    # GIT_COMMON_DIR is /path/to/main/.git
    get_filename_component(MAIN_REPO_ROOT "${GIT_COMMON_DIR}/.." ABSOLUTE)
    message(STATUS "Main repository root detected at: ${MAIN_REPO_ROOT}")
endif()

FILE(GLOB non_executable_SRC ${PROJECT_SOURCE_DIR}/src/**/*.cpp)
FILE(GLOB aiger_lib_SRC ${PROJECT_SOURCE_DIR}/libs/aiger/*.c)

add_executable(find_dependencies ${non_executable_SRC} ${aiger_lib_SRC} bins/find_dependencies.cpp)
add_executable(find_input_dependencies ${non_executable_SRC} ${aiger_lib_SRC} bins/find_input_dependencies.cpp)
add_executable(depsynt ${non_executable_SRC} ${aiger_lib_SRC} bins/synthesis.cpp)
add_executable(inp_dep_synthesis ${non_executable_SRC} ${aiger_lib_SRC} bins/inp_dep_synthesis.cpp)
add_executable(simulate_aiger ${non_executable_SRC} ${aiger_lib_SRC} bins/simulate_aiger.cpp)

list(APPEND CMAKE_PREFIX_PATH "./libs/spot" "./libs/abc" "${MAIN_REPO_ROOT}/libs/abc")
include_directories(./libs/abc/src ${MAIN_REPO_ROOT}/libs/abc/src)

# Dynamic Spot Include
find_path(SPOT_INCLUDE_DIR NAMES spot/twa/twa.hh 
          HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/spot/inst/include"
                "${CMAKE_CURRENT_SOURCE_DIR}/libs/spot/spot"
          NO_DEFAULT_PATH)

if (SPOT_INCLUDE_DIR)
    message(STATUS "Found Spot include dir: ${SPOT_INCLUDE_DIR}")
    include_directories(${SPOT_INCLUDE_DIR})
endif()
include_directories(${PROJECT_SOURCE_DIR}/src/findDeps ${PROJECT_SOURCE_DIR}/src/models ${PROJECT_SOURCE_DIR}/src/synthesis ${PROJECT_SOURCE_DIR}/src/utils ${PROJECT_SOURCE_DIR}/src/unates ${PROJECT_SOURCE_DIR}/libs/aiger ${PROJECT_SOURCE_DIR}/libs/abc ${PROJECT_SOURCE_DIR}/libs/abc/src ${PROJECT_SOURCE_DIR}/src/inputDependencies
                    ${MAIN_REPO_ROOT}/src/findDeps ${MAIN_REPO_ROOT}/src/models ${MAIN_REPO_ROOT}/src/synthesis ${MAIN_REPO_ROOT}/src/utils ${MAIN_REPO_ROOT}/src/unates ${MAIN_REPO_ROOT}/src/inputDependencies)

# Configuration
option(TEMP_DIRECTORY "Directory to store temporary files" "./")
configure_file(./src/utils/config.h.in ./src/utils/config.h)


# Define paths to look for Spot and ABC
set(SPOT_SEARCH_DIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/spot/inst/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/spot/inst")

set(ABC_SEARCH_DIRS 
    "${PROJECT_SOURCE_DIR}/libs/abc"
    "${MAIN_REPO_ROOT}/libs/abc"
    "/usr/local/lib"
    "/usr/lib")

# Include ABC package
find_library(abc_location NAMES libabc.so PATHS ${ABC_SEARCH_DIRS})
if (NOT abc_location)
    message(FATAL_ERROR "ABC library not found")
endif ()
message(STATUS "Found ABC location at ${abc_location}")
add_library(abc STATIC IMPORTED)
set_target_properties(abc PROPERTIES IMPORTED_LOCATION ${abc_location})

# Include Boots Options package
find_library(boost_options_location NAMES libboost_program_options.so)
if (NOT boost_options_location)
    message(FATAL_ERROR "Boost-options library not found")
endif ()
message(STATUS "Found BoostOptions location at ${boost_options_location}")
add_library(boost_options STATIC IMPORTED)
set_target_properties(boost_options PROPERTIES IMPORTED_LOCATION ${boost_options_location})

# Unified search for Spot/BDDX (Local in worktree -> Local in main -> System)
# Priorities: CMAKE_PREFIX_PATH (e.g. hpc_deps) -> Local libs/spot/inst -> System

# Include BDDX package
find_library(bddx_location NAMES bddx bddx.a HINTS ${SPOT_SEARCH_DIRS} NO_DEFAULT_PATH)
if (NOT bddx_location)
    find_library(bddx_location NAMES bddx bddx.a)
endif()
if (NOT bddx_location)
    message(FATAL_ERROR "BDDX library not found")
endif ()
message(STATUS "Found bddx location at ${bddx_location}")
add_library(bddx STATIC IMPORTED)
set_target_properties(bddx PROPERTIES IMPORTED_LOCATION ${bddx_location})

# Include spot package
find_library(spot_location NAMES spot spot.a HINTS ${SPOT_SEARCH_DIRS} NO_DEFAULT_PATH)
if (NOT spot_location)
    find_library(spot_location NAMES spot spot.a)
endif()
if (NOT spot_location)
    message(FATAL_ERROR "Spot library not found")
endif ()
message(STATUS "Found spot location at ${spot_location}")
add_library(spot STATIC IMPORTED)
set_target_properties(spot PROPERTIES IMPORTED_LOCATION ${spot_location})

# Include Spot headers
find_path(spot_include_dir_actual NAMES spot/twa/twa.hh 
          HINTS ${SPOT_SEARCH_DIRS} 
          PATH_SUFFIXES include 
          NO_DEFAULT_PATH)
if (NOT spot_include_dir_actual)
    set(spot_include_dir_actual ${SPOT_INCLUDE_DIR})
endif()

if (spot_include_dir_actual)
    message(STATUS "Using Spot include dir: ${spot_include_dir_actual}")
    include_directories(${spot_include_dir_actual})
endif()

# Linking exectuables to packages
target_link_libraries(find_dependencies boost_options spot bddx abc Threads::Threads)
target_link_libraries(find_input_dependencies boost_options spot bddx abc Threads::Threads)
target_link_libraries(depsynt boost_options spot bddx abc Threads::Threads)
target_link_libraries(inp_dep_synthesis boost_options spot bddx abc Threads::Threads)
target_link_libraries(simulate_aiger boost_options spot bddx abc Threads::Threads)
