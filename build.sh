#!/bin/bash

# TC375 Simulator Build Script

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo ""
    echo "======================================="
    echo "  TC375 Simulator Build Script"
    echo "======================================="
    echo ""
}

# Check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    if ! command -v cmake &> /dev/null; then
        print_error "cmake not found. Install with: brew install cmake"
        exit 1
    fi
    
    if ! brew list openssl &> /dev/null; then
        print_error "OpenSSL not found. Install with: brew install openssl"
        exit 1
    fi
    
    print_success "All dependencies found!"
}

# Build
build_project() {
    local BUILD_TYPE=${1:-Release}
    
    print_info "Build type: $BUILD_TYPE"
    
    mkdir -p build
    cd build
    
    print_info "Configuring with CMake..."
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
    
    print_info "Building..."
    local NUM_CORES=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    cmake --build . -j$NUM_CORES
    
    cd ..
    
    print_success "Build completed!"
    print_info "Executable: ./build/tc375_simulator"
}

# Clean
clean_build() {
    print_info "Cleaning build directory..."
    rm -rf build
    print_success "Clean completed!"
}

# Run
run_simulator() {
    if [ ! -f "build/tc375_simulator" ]; then
        print_error "Executable not found. Please build first."
        exit 1
    fi
    
    print_info "Running TC375 Simulator..."
    echo ""
    ./build/tc375_simulator "$@"
}

print_header

case "${1:-build}" in
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
    build)
        check_dependencies
        build_project "Release"
        ;;
    rebuild)
        clean_build
        check_dependencies
        build_project "Release"
        ;;
    run)
        if [ ! -f "build/tc375_simulator" ]; then
            check_dependencies
            build_project "Release"
        fi
        shift
        run_simulator "$@"
        ;;
    help|--help|-h)
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  build         Build in Release mode (default)"
        echo "  debug         Build in Debug mode"
        echo "  clean         Clean build directory"
        echo "  rebuild       Clean and rebuild"
        echo "  run [args]    Build (if needed) and run simulator"
        echo "  help          Show this help"
        echo ""
        ;;
    *)
        print_error "Unknown command: $1"
        echo "Use '$0 help' for usage"
        exit 1
        ;;
esac

echo ""

