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

# Find OpenCASCADE (OCE) packages
set(OCE_INCLUDE_DIRS "/usr/include/oce")

# Essential OCE libraries (only the ones we actually need and that exist)
set(OCE_LIBRARIES 
    TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKGeomAlgo
    TKTopAlgo TKPrim TKBO TKMesh TKShHealing TKBool TKFillet
    TKOffset TKService TKSTL TKSTEPBase TKSTEP TKIGES
    TKBinL TKXmlL TKCAF
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# Include OpenCASCADE headers as system headers to suppress their warnings
include_directories(SYSTEM ${OCE_INCLUDE_DIRS})

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Create the main library
add_library(semiconductor_device STATIC ${SOURCES} ${HEADERS})

# Target properties
target_include_directories(semiconductor_device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# Include OpenCASCADE headers as system headers to suppress their warnings
target_include_directories(semiconductor_device SYSTEM PUBLIC
    ${OCE_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(semiconductor_device
    ${OCE_LIBRARIES}
)

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
