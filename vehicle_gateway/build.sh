#!/bin/bash
# Build script for VMG

set -e

echo "Building VMG with PQC support..."

# Check OpenSSL version
openssl version | grep -q "OpenSSL 3" || {
    echo "Warning: OpenSSL 3.x recommended for full PQC support"
}

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo "Executables:"
echo "  - build/vmg_doip_server"
echo "  - build/vmg_https_client"
echo "  - build/vmg_mqtt_client"
echo ""
echo "Next steps:"
echo "  1. Generate certificates: ./scripts/generate_pqc_certs.sh"
echo "  2. Run DoIP server: ./build/vmg_doip_server ..."
echo "=========================================="

