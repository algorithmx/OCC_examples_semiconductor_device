#!/bin/bash

# Build script for OpenCASCADE Semiconductor Device Project
# Usage: ./build.sh [clean|debug|release]

set -e  # Exit on any error

PROJECT_ROOT=$(dirname "$(realpath "$0")")
BUILD_DIR="$PROJECT_ROOT/build"

# Function to print colored output
print_status() {
    echo -e "\033[1;32m[INFO]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

print_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check if cmake is available
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is not installed. Please install CMake first."
        exit 1
    fi
    
    # Check if make is available
    if ! command -v make &> /dev/null; then
        print_error "Make is not installed. Please install build tools first."
        exit 1
    fi
    
    # Check if OpenCASCADE/OCCT (libocct) headers are present in common system paths
    if [ ! -d "/usr/include/opencascade" ] && [ ! -d "/usr/include/occt" ] && [ ! -d "/usr/include/OCCT" ]; then
        print_warning "OpenCASCADE/OCCT (libocct) headers not found in common include paths."
        print_warning "CMake will still attempt to find OCCT libraries on the system; if you see link errors, install OCCT dev packages:" 
        print_warning "  sudo apt update"
        print_warning "  sudo apt install libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev libocct-ocaf-dev libocct-visualization-dev"
        print_warning "Optional helpful packages: libocct-data-exchange-dev libocct-draw-dev"
    fi
    
    print_status "All dependencies found."
}

# Function to clean build directory
clean_build() {
    print_status "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_status "Build directory cleaned."
    else
        print_status "Build directory doesn't exist, nothing to clean."
    fi
}

# Function to configure and build
build_project() {
    local build_type=${1:-Debug}
    
    print_status "Building project in $build_type mode..."
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake (use absolute path to project root to avoid cwd issues)
    print_status "Configuring with CMake..."
    cmake -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        "$PROJECT_ROOT"

    # Build the project using CMake --build so generators are respected
    print_status "Building (parallel jobs=$(nproc))..."
    cmake --build . -- -j$(nproc)
    
    print_status "Build completed successfully!"
}

# Function to run tests
run_examples() {
    print_status "Running example applications..."
    
    cd "$BUILD_DIR"
    
    if [ -f "examples/basic_shapes_example" ]; then
        print_status "Running basic shapes example..."
        cd examples
        ./basic_shapes_example
        cd ..
    else
        print_warning "basic_shapes_example not found, skipping..."
    fi
    
    if [ -f "examples/mosfet_example" ]; then
        print_status "Running MOSFET example..."
        cd examples
        ./mosfet_example
        cd ..
    else
        print_warning "mosfet_example not found, skipping..."
    fi
}

# Function to install the project
install_project() {
    print_status "Installing project..."
    
    cd "$BUILD_DIR"
    sudo make install
    
    print_status "Project installed successfully!"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  clean          Clean the build directory"
    echo "  debug          Build in Debug mode (default)"
    echo "  release        Build in Release mode"
    echo "  install        Install the project after building"
    echo "  examples       Run example applications"
    echo "  help           Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0              # Build in Debug mode"
    echo "  $0 release      # Build in Release mode"
    echo "  $0 clean        # Clean build directory"
    echo "  $0 examples     # Run example applications"
}

# Main script
main() {
    local command=${1:-debug}
    
    case "$command" in
        clean)
            clean_build
            ;;
        debug)
            check_dependencies
            build_project "Debug"
            ;;
        release)
            check_dependencies
            build_project "Release"
            ;;
        install)
            check_dependencies
            build_project "Release"
            install_project
            ;;
        examples)
            if [ ! -d "$BUILD_DIR" ]; then
                print_error "Project not built yet. Run './build.sh' first."
                exit 1
            fi
            run_examples
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            print_error "Unknown option: $command"
            show_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"
