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
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

## Simplified OCCT handling: always use system include paths and plain library names.
# The following cache variables can be configured by the user or the build system:
# - OCCT_INCLUDE_DIR: path to OpenCASCADE/OCCT headers (e.g. /usr/include/opencascade)
# - OCCT_LIB_DIR: path to OCCT libraries (optional, used by find_library)
# - OCCT_LIB_NAMES: semicolon-separated list of OCCT library basenames to link (defaults below)

set(OCCT_INCLUDE_DIR "" CACHE PATH "Path to OCCT include directory (system headers)")
set(OCCT_LIB_DIR "" CACHE PATH "Optional path to OCCT libraries (used by find_library)")
set(OCCT_LIB_NAMES "TKBRep;TKGeomAlgo;TKTopAlgo;TKPrim;TKMath;TKG2d;TKG3d;TKGeomBase;TKernel;TKService;TKShHealing;TKBool;TKOffset;TKSTL;TKSTEP;TKSTEPBase;TKIGES;TKXmlL;TKBinL;TKCAF;TKMesh" CACHE STRING "OCCT library basenames to link")

# If user did not set OCCT_INCLUDE_DIR, try common system locations.
if(NOT OCCT_INCLUDE_DIR)
    if(EXISTS "/usr/include/opencascade")
        set(OCCT_INCLUDE_DIR "/usr/include/opencascade")
    elseif(EXISTS "/usr/include/occt")
        set(OCCT_INCLUDE_DIR "/usr/include/occt")
    endif()
endif()

# Include directories are set per-target below (use target_include_directories on targets)

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

# Link libraries: use system include dir and find OCCT libs by basename.
if(OCCT_INCLUDE_DIR)
    target_include_directories(semiconductor_device SYSTEM PUBLIC ${OCCT_INCLUDE_DIR})
endif()

# Resolve OCCT libraries listed in OCCT_LIB_NAMES using OCCT_LIB_DIR (if provided)
set(OCCT_LIBS "")
foreach(_lib ${OCCT_LIB_NAMES})
    # try common name first (lib<name>.so) in OCCT_LIB_DIR then system paths
    if(OCCT_LIB_DIR)
        find_library(_found_${_lib} NAMES ${_lib} lib${_lib} PATHS ${OCCT_LIB_DIR} NO_DEFAULT_PATH)
    else()
        find_library(_found_${_lib} NAMES ${_lib} lib${_lib} PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /lib)
    endif()

    if(_found_${_lib})
        list(APPEND OCCT_LIBS ${_found_${_lib}})
    else()
        # fall back to bare name so linker may resolve it in default system paths
        list(APPEND OCCT_LIBS ${_lib})
    endif()
endforeach()

if(OCCT_LIBS)
    target_link_libraries(semiconductor_device PUBLIC ${OCCT_LIBS})
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
