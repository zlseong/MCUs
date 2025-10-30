#!/bin/bash

##
## TC375 Dual-Bank Bootloader Build Example
## 동일한 소스를 A/B 메모리에 각각 빌드
##

set -e

# Toolchain
TRICORE_GCC=tricore-gcc
TRICORE_OBJCOPY=tricore-objcopy

# Directories
COMMON_DIR="../tc375_bootloader/common"
BOOTLOADER_DIR="../tc375_bootloader/bootloader"
BUILD_DIR="./build"

# Create build directory
mkdir -p $BUILD_DIR

echo "=================================================="
echo "Building TC375 Dual-Bank Bootloader"
echo "=================================================="
echo ""

# ============================================================================
# Stage 2A (Region A: 0x800A1000)
# ============================================================================

echo "[1/4] Compiling Stage 2A (Region A)..."

# 동일한 소스 파일 컴파일
$TRICORE_GCC -c \
    $BOOTLOADER_DIR/stage2_main.c \
    $COMMON_DIR/boot_common.c \
    $COMMON_DIR/doip_client.c \
    $COMMON_DIR/doip_message.c \
    $COMMON_DIR/uds_handler.c \
    -I$COMMON_DIR \
    -O2 -Wall \
    -o $BUILD_DIR/stage2a_temp.o

echo "[2/4] Linking Stage 2A with Region A linker script..."

# Region A 링커 스크립트로 링크
$TRICORE_GCC \
    $BUILD_DIR/stage2a_temp.o \
    -T $BOOTLOADER_DIR/stage2a_linker.ld \
    -o $BUILD_DIR/stage2a.elf

# ELF → HEX 변환
$TRICORE_OBJCOPY -O ihex $BUILD_DIR/stage2a.elf $BUILD_DIR/stage2a.hex

echo "  → stage2a.elf created (@ 0x800A1000)"
echo "  → stage2a.hex created"
echo ""

# ============================================================================
# Stage 2B (Region B: 0x820A1000)
# ============================================================================

echo "[3/4] Compiling Stage 2B (Region B)..."

# 동일한 소스 파일 컴파일 (Stage 2A와 완전히 동일!)
$TRICORE_GCC -c \
    $BOOTLOADER_DIR/stage2_main.c \
    $COMMON_DIR/boot_common.c \
    $COMMON_DIR/doip_client.c \
    $COMMON_DIR/doip_message.c \
    $COMMON_DIR/uds_handler.c \
    -I$COMMON_DIR \
    -O2 -Wall \
    -o $BUILD_DIR/stage2b_temp.o

echo "[4/4] Linking Stage 2B with Region B linker script..."

# Region B 링커 스크립트로 링크 (링커만 다름!)
$TRICORE_GCC \
    $BUILD_DIR/stage2b_temp.o \
    -T $BOOTLOADER_DIR/stage2b_linker.ld \
    -o $BUILD_DIR/stage2b.elf

# ELF → HEX 변환
$TRICORE_OBJCOPY -O ihex $BUILD_DIR/stage2b.elf $BUILD_DIR/stage2b.hex

echo "  → stage2b.elf created (@ 0x820A1000)"
echo "  → stage2b.hex created"
echo ""

# ============================================================================
# Summary
# ============================================================================

echo "=================================================="
echo "Build Complete!"
echo "=================================================="
echo ""
echo "Output files:"
echo "  - $BUILD_DIR/stage2a.elf (Region A: 0x800A1000)"
echo "  - $BUILD_DIR/stage2a.hex"
echo "  - $BUILD_DIR/stage2b.elf (Region B: 0x820A1000)"
echo "  - $BUILD_DIR/stage2b.hex"
echo ""
echo "Memory Layout:"
echo "  Region A (0x80000000):"
echo "    ├─ 0x80000100  SSW (64KB)"
echo "    ├─ 0x800A0000  Stage 2 Metadata"
echo "    ├─ 0x800A1000  Stage 2A ← stage2a.elf"
echo "    └─ 0x800D3000  Application A"
echo ""
echo "  Region B (0x82000000):"
echo "    ├─ 0x82000100  SSW (64KB)"
echo "    ├─ 0x820A0000  Stage 2 Metadata"
echo "    ├─ 0x820A1000  Stage 2B ← stage2b.elf"
echo "    └─ 0x820D3000  Application B"
echo ""
echo "Note: Stage 2A and Stage 2B have identical code,"
echo "      but are linked to different memory addresses!"
echo ""

# Cleanup temp files
rm -f $BUILD_DIR/*.o

echo "Done!"

