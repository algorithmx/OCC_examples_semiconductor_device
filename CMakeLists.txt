cmake_minimum_required(VERSION 3.16)
project(SemiconductorDevice VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Suppress noisy warnings from system headers (OpenCASCADE)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wno-attributes
        -Wno-unknown-pragmas
        -Wno-extra  # Suppress extra warnings from system headers
    )
endif()

# Additional warning suppressions for OpenCASCADE specifically
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wno-deprecated-declarations
        -Wno-unused-parameter
        -Wno-sign-compare
    )
endif()

# Find OpenCASCADE / OCCT (libocct) packages using CMake's FindPackage
# Prefer OCCT CMake config if available
find_package(OCCT CONFIG)

# OCCT provides imported targets when installed with CMake config files.
# If not available, we'll fall back to using system include paths and common library names.
set(OCCT_FALLBACK_INCLUDE_DIR "")
if(NOT OCCT_FOUND)
    message(STATUS "OCCT package config not found via find_package(OCCT). Falling back to system include paths and library names.")
    if(EXISTS "/usr/include/opencascade")
        set(OCCT_FALLBACK_INCLUDE_DIR "/usr/include/opencascade")
    elseif(EXISTS "/usr/include/occt")
        set(OCCT_FALLBACK_INCLUDE_DIR "/usr/include/occt")
    endif()
endif()
set(OCCT_COMPONENTS
    Foundation
    Math
    TKernel
    TKGeomBase
    TKG2d
    TKG3d
    TKGeomAlgo
    TKBRep
    TKTopAlgo
    TKPrim
    TKMesh
    TKShHealing
    TKBool
    TKOffset
    TKService
    TKSTL
    TKSTEP
    TKIGES
    TKSTEPBase
    TKXmlL
    TKBinL
    TKCAF
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# Note: OCCT's CMake config provides include directories via imported targets.

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Create the main library (shared so OCCT shared deps propagate to executables)
add_library(semiconductor_device SHARED ${SOURCES} ${HEADERS})
set_target_properties(semiconductor_device PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Target properties
target_include_directories(semiconductor_device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Include OpenCASCADE headers as system headers to suppress their warnings
# No need to add raw include dirs; link the OCCT targets below which carry include dirs.

# Link libraries
# Link against OCCT targets. If the package provides imported targets, prefer them.
foreach(comp IN LISTS OCCT_COMPONENTS)
    string(TOUPPER "${comp}" COMP_UPPER)
    # Try to link the corresponding libocct target if available
    if(TARGET OCCT::${comp})
        target_link_libraries(semiconductor_device PUBLIC OCCT::${comp})
    endif()
endforeach()

# Fallback: if OCCT package config wasn't found, add include dir and link common libocct libraries
if(NOT OCCT_FOUND)
    if(OCCT_FALLBACK_INCLUDE_DIR)
        target_include_directories(semiconductor_device SYSTEM PUBLIC ${OCCT_FALLBACK_INCLUDE_DIR})
    endif()

    # Robust fallback: try to locate each OCCT library individually in common lib paths.
    set(_occt_lib_names
        TKBRep
        TKGeomAlgo
        TKTopAlgo
        TKPrim
        TKMath
        TKG2d
        TKG3d
        TKGeomBase
        TKernel
        TKService
        TKShHealing
        TKBool
        TKOffset
        TKSTL
        TKSTEP
        TKSTEPBase
        TKIGES
        TKXmlL
        TKBinL
        TKCAF
        TKMesh
    )

    set(OCCT_LIBS "")
    foreach(_lib ${_occt_lib_names})
        find_library(_found_${_lib} NAMES ${_lib} lib${_lib} PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /lib NO_DEFAULT_PATH)
        if(_found_${_lib})
            list(APPEND OCCT_LIBS ${_found_${_lib}})
        else()
            # If not found by full path, append bare name so the linker may still find it in default paths
            list(APPEND OCCT_LIBS ${_lib})
        endif()
    endforeach()

    message(STATUS "OCCT fallback link list: ${OCCT_LIBS}")
    # Create an INTERFACE target to carry OCCT link and include flags to consumers
    add_library(occt_fallback INTERFACE)
    target_link_libraries(occt_fallback INTERFACE ${OCCT_LIBS})
    if(OCCT_FALLBACK_INCLUDE_DIR)
        target_include_directories(occt_fallback INTERFACE ${OCCT_FALLBACK_INCLUDE_DIR})
    endif()

    # Link the main library against the occt_fallback so executables linking it inherit the OCCT libs
    target_link_libraries(semiconductor_device PUBLIC occt_fallback)
endif()

# Add examples subdirectory
add_subdirectory(examples)

# Add tests subdirectory if it exists and has CMakeLists.txt
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS semiconductor_device
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
