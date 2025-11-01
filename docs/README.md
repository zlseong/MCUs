# VMG and MCUs Documentation

**Organized Documentation for Vehicle OTA System with PQC-Hybrid TLS**

---

## Document Structure

```
docs/
├── bootloader/           # TC375 Bootloader & Dual-Bank
├── network/              # DoIP, TLS, Communication Protocols
├── mcu/                  # TC375 MCU Firmware Architecture
├── ota/                  # OTA Update Scenarios
├── diagnostics/          # Remote Diagnostics & UDS
├── system/               # System Architecture & Integration
├── architecture/         # High-level System Overview
└── QUICK_REFERENCE.md    # Quick Reference Guide
```

---

## 📚 Documentation by Category

### 1. Bootloader (TC375 Dual-Bank)
**Path:** `bootloader/`

- **tc375_bootloader_guide.md** - Complete guide for TC375 bootloader
  - Infineon SSW (Startup Software) = Stage 1
  - Application Bootloader = Stage 2 (A/B)
  - User Application = App (A/B)
  - Dual-bank OTA update flow
  - Fail-safe mechanisms

---

### 2. Network & Communication
**Path:** `network/`

- **ISO_13400_specification.md** - DoIP (Diagnostics over IP) standard
  - ISO 13400-2 protocol specification
  - Connection-oriented TCP communication
  - Payload types and message formats
  - Security considerations (TLS optional but recommended)

- **MBEDTLS_HANDSHAKE_TIMING.md** - mbedTLS handshake timing in in-vehicle network
  - VMG, Zonal Gateway, ECU boot sequences
  - TLS handshake timing (when it occurs)
  - DoIP Server/Client initialization
  - Connection establishment flow

---

### 3. MCU (TC375 Firmware)
**Path:** `mcu/`

- **tc375_firmware_architecture.md** - Complete TC375 firmware architecture
  - UDS (Unified Diagnostic Services)
  - Transaction & Rollback mechanisms
  - Dual-bank (A/B) architecture
  - Flash memory layout (6 MB PFLASH)
  - OTA update flow (Download, Verify, Install, Rollback)
  - Fail-safe strategies (Watchdog, Fallback, Power loss)

- **TC375_HSM_INTEGRATION.md** - Hardware Security Module integration
  - HSM (Hardware Security Module) for TC375
  - Secure boot with HSM
  - Key storage and cryptographic operations
  - PQC signature verification using HSM

---

### 4. OTA (Over-The-Air Updates)
**Path:** `ota/`

- **ota_scenario_detailed.md** - 4-phase hierarchical OTA update scenario
  - Phase 1: Package Transfer (Server → VMG → ZG → ECU)
  - Phase 2: VCI Collection & Readiness Check
  - Phase 3: Activation (Driver approval required)
  - Phase 4: Result Reporting
  - Dual-bank strategy and rollback support

---

### 5. Diagnostics (Remote & UDS)
**Path:** `diagnostics/`

- **REMOTE_DIAGNOSTICS_ARCHITECTURE.md** - Remote diagnostics system architecture
  - Message flow: Server → VMG → ZG → ECU
  - DoIP message routing
  - Broadcast diagnostics (zone-wide)
  - Timeout and retry mechanisms

- **REMOTE_DIAGNOSTICS_USAGE.md** - Usage guide for remote diagnostics
  - API endpoints (POST /api/v1/diagnostics/send)
  - UDS service examples (0x22, 0x19, 0x11, etc.)
  - Python examples
  - Error handling

- **UDS_IMPLEMENTATION_STATUS.md** - UDS service implementation status
  - Fully implemented: 0x10, 0x11, 0x22, 0x27, 0x2E, 0x31, 0x34, 0x36, 0x37, 0x3E
  - Partially implemented: 0x14, 0x19, 0x28, 0x85
  - Not implemented: 0x23, 0x24, 0x2C, 0x2F, 0x83, 0x84, 0x86, 0x87

---

### 6. System Architecture & Integration
**Path:** `system/`

- **BOOT_AND_RUNTIME_COMPARISON.md** - Boot and runtime comparison
  - OTA Server (Python on Ubuntu)
  - VMG (C++ on Linux)
  - Zonal Gateway (C on TC375)
  - End Node ECU (C on TC375)
  - Boot sequences and operational patterns

- **zonal_gateway_architecture.md** - Zonal Gateway architecture
  - Dual role: Server (for ECUs) + Client (for VMG)
  - Zone management
  - Message routing and aggregation
  - Reconnection logic

- **vmg_dynamic_ip_management.md** - VMG dynamic IP management
  - Problem: Vehicle moves → IP changes → Connection lost
  - Solution: MQTT with Clean Session = False + Auto-reconnection
  - Exponential backoff (1s → 60s)
  - Keepalive and session persistence

- **unified_message_format.md** - Unified message format for all components
  - JSON message structures
  - MQTT topics
  - DoIP message formats
  - VCI (Vehicle Configuration Information)

---

### 7. High-Level Architecture
**Path:** `architecture/`

- **system_overview.md** - Complete system overview
  - Component roles (Server, VMG, ZG, ECU)
  - Communication protocols (MQTT, HTTPS, DoIP)
  - Security (PQC-TLS for external, mbedTLS for in-vehicle)

---

### 8. Quick Reference
**Path:** `./`

- **QUICK_REFERENCE.md** - Quick reference for common tasks
  - Command cheat sheet
  - API endpoints
  - UDS service IDs
  - Memory addresses
  - Port numbers

---

## 🔍 Finding What You Need

### I want to understand...

#### Bootloader & OTA Updates
→ `bootloader/tc375_bootloader_guide.md`  
→ `ota/ota_scenario_detailed.md`  
→ `mcu/tc375_firmware_architecture.md`

#### Network Communication
→ `network/ISO_13400_specification.md` (DoIP standard)  
→ `network/MBEDTLS_HANDSHAKE_TIMING.md` (TLS timing)

#### Remote Diagnostics
→ `diagnostics/REMOTE_DIAGNOSTICS_ARCHITECTURE.md` (architecture)  
→ `diagnostics/REMOTE_DIAGNOSTICS_USAGE.md` (how to use)  
→ `diagnostics/UDS_IMPLEMENTATION_STATUS.md` (what's implemented)

#### System Architecture
→ `architecture/system_overview.md` (high-level)  
→ `system/zonal_gateway_architecture.md` (ZG details)  
→ `system/BOOT_AND_RUNTIME_COMPARISON.md` (component comparison)

#### VMG Specific
→ `system/vmg_dynamic_ip_management.md` (IP changes)  
→ `system/unified_message_format.md` (message formats)

#### TC375 MCU
→ `mcu/tc375_firmware_architecture.md` (firmware)  
→ `mcu/TC375_HSM_INTEGRATION.md` (security)  
→ `bootloader/tc375_bootloader_guide.md` (bootloader)

---

## 📊 Document Statistics

**Total Documents: 12** (reduced from 24)

| Category | Count | Description |
|----------|-------|-------------|
| Bootloader | 1 | TC375 dual-bank bootloader |
| Network | 2 | DoIP + mbedTLS |
| MCU | 2 | Firmware + HSM |
| OTA | 1 | 4-phase OTA scenario |
| Diagnostics | 3 | Architecture + Usage + Status |
| System | 4 | Architecture + Integration |
| Architecture | 1 | High-level overview |
| Reference | 1 | Quick reference |

---

## 🚀 Getting Started

### For Developers

1. **Start here:** `architecture/system_overview.md`
2. **Then read:** `QUICK_REFERENCE.md`
3. **Deep dive:** Choose category based on your task

### For OTA Implementation

1. `ota/ota_scenario_detailed.md` - Understand the flow
2. `bootloader/tc375_bootloader_guide.md` - Understand dual-bank
3. `mcu/tc375_firmware_architecture.md` - Understand firmware

### For Diagnostics

1. `diagnostics/REMOTE_DIAGNOSTICS_ARCHITECTURE.md` - Understand architecture
2. `diagnostics/REMOTE_DIAGNOSTICS_USAGE.md` - Learn how to use
3. `diagnostics/UDS_IMPLEMENTATION_STATUS.md` - Check what's available

### For Network Communication

1. `network/ISO_13400_specification.md` - Learn DoIP
2. `network/MBEDTLS_HANDSHAKE_TIMING.md` - Understand TLS timing
3. `system/vmg_dynamic_ip_management.md` - Handle IP changes

---

## 📝 Document Conventions

### File Naming
- **Lowercase with underscores:** `tc375_bootloader_guide.md`
- **UPPERCASE for acronyms:** `UDS_IMPLEMENTATION_STATUS.md`
- **Descriptive names:** `vmg_dynamic_ip_management.md`

### Content Structure
- **Markdown format:** All documents use Markdown
- **Code blocks:** Use triple backticks with language tags
- **Diagrams:** ASCII art for architecture diagrams
- **Tables:** For comparisons and specifications

### Language
- **Primary:** English
- **Technical terms:** Use industry-standard terminology
- **Acronyms:** Define on first use

---

## 🔄 Document Updates

### When to Update
- New features implemented
- Architecture changes
- Bug fixes that affect design
- New requirements

### How to Update
1. Edit the relevant document
2. Update version/date at top of document
3. Add changelog entry if significant
4. Update this README if structure changes

---

## 📚 Related Documentation

### External References
- [Infineon TC375 Documentation](https://www.infineon.com/cms/en/product/microcontroller/32-bit-tricore-microcontroller/32-bit-tricore-aurix-tc3xx/)
- [ISO 13400 DoIP Standard](https://www.iso.org/standard/55283.html)
- [ISO 14229 UDS Standard](https://www.iso.org/standard/72439.html)
- [NIST PQC Standards](https://csrc.nist.gov/projects/post-quantum-cryptography)

### Project Documentation
- Main README: `../README.md`
- API Documentation: `../api/`
- Code Examples: `../examples/`

---

## 💡 Tips

### Searching
- Use your IDE's search (Ctrl+Shift+F) to find across all docs
- Search for acronyms (e.g., "UDS", "DoIP", "OTA")
- Search for addresses (e.g., "0x80000000")

### Navigation
- Use your IDE's file tree
- Use "Go to Definition" for cross-references
- Bookmark frequently used documents

### Contributing
- Keep documents up to date
- Use consistent formatting
- Add examples where helpful
- Link related documents

---

## 📞 Support

For questions or issues:
1. Check `QUICK_REFERENCE.md` first
2. Search relevant category documentation
3. Check code comments in implementation
4. Refer to external standards

---

**Last Updated:** 2025-11-01  
**Document Version:** 2.0 (Reorganized and consolidated)

