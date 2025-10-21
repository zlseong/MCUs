#!/bin/bash

# TC375 Bootloader Build Script
# Builds both Stage 1 and Stage 2 (A/B)

set -e

echo "======================================="
echo "  TC375 2-Stage Bootloader Builder"
echo "======================================="
echo ""

# Check for TriCore GCC
if ! command -v tricore-gcc &> /dev/null; then
    echo "❌ tricore-gcc not found"
    echo "This requires AURIX Development Studio (ADS) toolchain"
    echo "For now, this is a TEMPLATE for TC375 development"
    echo ""
    echo "On Windows with ADS, use:"
    echo "  tricore-gcc instead of gcc"
    echo ""
    exit 1
fi

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# Build Stage 1 (64 KB, immutable)
echo -e "${BLUE}[1/3] Building Stage 1 Bootloader...${NC}"
cd stage1
tricore-gcc -c stage1_main.c -o stage1_main.o -Os -Wall
tricore-ld -T stage1_linker.ld -o stage1_boot.elf stage1_main.o
tricore-objcopy -O ihex stage1_boot.elf stage1_boot.hex
echo -e "${GREEN}✅ Stage 1 built: stage1/stage1_boot.hex${NC}"
cd ..

# Build Stage 2A (188 KB)
echo -e "${BLUE}[2/3] Building Stage 2A Bootloader...${NC}"
cd stage2
tricore-gcc -DSTAGE2_A -c stage2_main.c -o stage2a_main.o -Os -Wall
tricore-ld -T stage2_linker.ld -o stage2a_boot.elf stage2a_main.o
tricore-objcopy -O ihex stage2a_boot.elf stage2a_boot.hex
echo -e "${GREEN}✅ Stage 2A built: stage2/stage2a_boot.hex${NC}"

# Build Stage 2B (188 KB, same code)
echo -e "${BLUE}[3/3] Building Stage 2B Bootloader...${NC}"
tricore-gcc -DSTAGE2_B -c stage2_main.c -o stage2b_main.o -Os -Wall
tricore-ld -T stage2_linker.ld -o stage2b_boot.elf stage2b_main.o
tricore-objcopy -O ihex stage2b_boot.elf stage2b_boot.hex
echo -e "${GREEN}✅ Stage 2B built: stage2/stage2b_boot.hex${NC}"
cd ..

echo ""
echo "======================================="
echo "  Build Complete!"
echo "======================================="
echo ""
echo "Flash to TC375:"
echo "  1. Stage 1: flash_tool write 0x80000000 stage1/stage1_boot.hex"
echo "  2. Stage 2A: flash_tool write 0x80011000 stage2/stage2a_boot.hex"
echo "  3. Stage 2B: flash_tool write 0x80041000 stage2/stage2b_boot.hex"
echo ""

