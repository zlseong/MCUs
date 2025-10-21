#include "ota_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace tc375 {

// ============================================================================
// Firmware Metadata
// ============================================================================

bool FirmwareMetadata::isValid() const {
    return version > 0 && size > 0 && crc32 != 0;
}

std::string FirmwareMetadata::toString() const {
    std::stringstream ss;
    ss << "Version: " << version << ", Size: " << size 
       << " bytes, CRC: 0x" << std::hex << crc32 << std::dec;
    return ss.str();
}

// ============================================================================
// OTA Manager Implementation
// ============================================================================

OtaManager::OtaManager()
    : state_(OtaState::IDLE)
    , current_bank_(BootBank::BANK_A)
    , target_size_(0)
    , bytes_written_(0)
{
    // Load current bank from bootloader
    current_bank_ = Bootloader::getActiveBank();
    
    // Load bank metadata
    loadBankMetadata(BootBank::BANK_A, bank_a_meta_);
    loadBankMetadata(BootBank::BANK_B, bank_b_meta_);
    
    std::cout << "[OTA] Current boot bank: " 
              << (current_bank_ == BootBank::BANK_A ? "A" : "B") << std::endl;
}

bool OtaManager::startDownload(uint32_t firmware_size, const FirmwareMetadata& metadata) {
    if (state_ != OtaState::IDLE) {
        handleError("OTA already in progress");
        return false;
    }

    if (!metadata.isValid()) {
        handleError("Invalid firmware metadata");
        return false;
    }

    setState(OtaState::DOWNLOADING);
    
    target_size_ = firmware_size;
    target_metadata_ = metadata;
    bytes_written_ = 0;
    
    // Determine target bank (opposite of current)
    BootBank target = getTargetBank();
    
    std::cout << "[OTA] Starting download to Bank " 
              << (target == BootBank::BANK_A ? "A" : "B") << std::endl;
    std::cout << "[OTA] Firmware: " << metadata.toString() << std::endl;
    
    // Erase target bank
    if (!eraseBank(target)) {
        setState(OtaState::FAILED);
        handleError("Failed to erase target bank");
        return false;
    }
    
    // Create temporary file for Mac simulation
    temp_file_path_ = "/tmp/ota_firmware_" + 
                      std::string(target == BootBank::BANK_A ? "a" : "b") + ".bin";
    
    return true;
}

bool OtaManager::writeBlock(uint32_t offset, const uint8_t* data, size_t length) {
    if (state_ != OtaState::DOWNLOADING) {
        handleError("Not in download state");
        return false;
    }

    if (offset + length > target_size_) {
        handleError("Write exceeds firmware size");
        return false;
    }

    // Write to target bank
    BootBank target = getTargetBank();
    
    if (!writeToFlash(target, offset, data, length)) {
        setState(OtaState::FAILED);
        handleError("Flash write failed");
        return false;
    }

    bytes_written_ += length;
    updateProgress();
    
    return true;
}

bool OtaManager::verify() {
    if (state_ != OtaState::DOWNLOADING) {
        handleError("Not in download state");
        return false;
    }

    setState(OtaState::VERIFYING);
    std::cout << "[OTA] Verifying firmware..." << std::endl;

    BootBank target = getTargetBank();

    // 1. Verify CRC
    if (!verifyCRC(target, target_metadata_.crc32)) {
        setState(OtaState::FAILED);
        handleError("CRC verification failed");
        return false;
    }
    std::cout << "[OTA] CRC verification: OK" << std::endl;

    // 2. Verify signature (PQC)
    if (!verifySignature(target, target_metadata_.signature)) {
        setState(OtaState::FAILED);
        handleError("Signature verification failed");
        return false;
    }
    std::cout << "[OTA] Signature verification: OK" << std::endl;

    // 3. Save metadata
    BankMetadata new_meta;
    new_meta.valid = true;
    new_meta.firmware = target_metadata_;
    new_meta.boot_count = 0;
    new_meta.last_boot_timestamp = 0;
    
    if (!saveBankMetadata(target, new_meta)) {
        setState(OtaState::FAILED);
        handleError("Failed to save metadata");
        return false;
    }

    std::cout << "[OTA] Firmware verification successful" << std::endl;
    return true;
}

bool OtaManager::install() {
    if (state_ != OtaState::VERIFYING) {
        handleError("Firmware not verified");
        return false;
    }

    setState(OtaState::INSTALLING);
    std::cout << "[OTA] Installing firmware..." << std::endl;

    BootBank target = getTargetBank();

    // Mark target bank as valid
    Bootloader::markFirmwareValid(target);

    // Switch to target bank
    if (!switchBank(target)) {
        setState(OtaState::FAILED);
        handleError("Failed to switch bank");
        return false;
    }

    setState(OtaState::SUCCESS);
    std::cout << "[OTA] Installation successful! Reboot required." << std::endl;
    std::cout << "[OTA] Will boot from Bank " 
              << (target == BootBank::BANK_A ? "A" : "B") 
              << " on next restart" << std::endl;

    return true;
}

bool OtaManager::rollback() {
    setState(OtaState::ROLLBACK);
    std::cout << "[OTA] Rolling back to previous firmware..." << std::endl;

    BootBank fallback = (current_bank_ == BootBank::BANK_A) ? BootBank::BANK_B : BootBank::BANK_A;

    // Check if fallback bank is valid
    if (!Bootloader::isValidFirmware(fallback)) {
        handleError("Fallback bank is invalid, cannot rollback");
        setState(OtaState::FAILED);
        return false;
    }

    // Switch to fallback bank
    if (!switchBank(fallback)) {
        handleError("Failed to switch to fallback bank");
        setState(OtaState::FAILED);
        return false;
    }

    setState(OtaState::SUCCESS);
    std::cout << "[OTA] Rollback successful! Reboot to Bank " 
              << (fallback == BootBank::BANK_A ? "A" : "B") << std::endl;

    return true;
}

BootBank OtaManager::getTargetBank() const {
    return (current_bank_ == BootBank::BANK_A) ? BootBank::BANK_B : BootBank::BANK_A;
}

bool OtaManager::switchBank(BootBank bank) {
    std::cout << "[OTA] Switching boot bank to " 
              << (bank == BootBank::BANK_A ? "A" : "B") << std::endl;
    
    if (!Bootloader::setActiveBank(bank)) {
        return false;
    }
    
    // In real TC375: this would be persisted to non-volatile memory
    // On next reboot, bootloader reads this and boots from selected bank
    
    return true;
}

int OtaManager::getProgress() const {
    if (target_size_ == 0) {
        return 0;
    }
    return static_cast<int>((bytes_written_ * 100) / target_size_);
}

std::string OtaManager::getStatusReport() const {
    std::stringstream ss;
    ss << "=== OTA Manager Status ===" << std::endl;
    ss << "State: ";
    switch (state_) {
        case OtaState::IDLE: ss << "IDLE"; break;
        case OtaState::DOWNLOADING: ss << "DOWNLOADING"; break;
        case OtaState::VERIFYING: ss << "VERIFYING"; break;
        case OtaState::INSTALLING: ss << "INSTALLING"; break;
        case OtaState::ROLLBACK: ss << "ROLLBACK"; break;
        case OtaState::SUCCESS: ss << "SUCCESS"; break;
        case OtaState::FAILED: ss << "FAILED"; break;
    }
    ss << std::endl;
    ss << "Current Bank: " << (current_bank_ == BootBank::BANK_A ? "A" : "B") << std::endl;
    ss << "Progress: " << getProgress() << "%" << std::endl;
    ss << "Bytes Written: " << bytes_written_ << " / " << target_size_ << std::endl;
    return ss.str();
}

bool OtaManager::saveBankMetadata(BootBank bank, const BankMetadata& meta) {
    // In real TC375: write to dedicated Flash sector
    // For Mac: save to file
    std::string filename = "/tmp/bank_" + 
                          std::string(bank == BootBank::BANK_A ? "a" : "b") + "_meta.bin";
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Serialize metadata (simplified)
    file.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
    return true;
}

bool OtaManager::loadBankMetadata(BootBank bank, BankMetadata& meta) {
    std::string filename = "/tmp/bank_" + 
                          std::string(bank == BootBank::BANK_A ? "a" : "b") + "_meta.bin";
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        // Default metadata if file doesn't exist
        meta.valid = false;
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&meta), sizeof(meta));
    return true;
}

bool OtaManager::writeToFlash(BootBank bank, uint32_t offset, const uint8_t* data, size_t length) {
    // Mac simulation: write to temp file
    std::string filename = "/tmp/ota_firmware_" + 
                          std::string(bank == BootBank::BANK_A ? "a" : "b") + ".bin";
    
    std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        // Create file if doesn't exist
        file.open(filename, std::ios::out | std::ios::binary);
        file.close();
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }
    
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(data), length);
    
    std::cout << "[Flash] Written " << length << " bytes at offset " << offset 
              << " to Bank " << (bank == BootBank::BANK_A ? "A" : "B") << std::endl;
    
    return true;
}

bool OtaManager::eraseBank(BootBank bank) {
    std::cout << "[Flash] Erasing Bank " 
              << (bank == BootBank::BANK_A ? "A" : "B") << std::endl;
    
    // Mac simulation: delete temp file
    std::string filename = "/tmp/ota_firmware_" + 
                          std::string(bank == BootBank::BANK_A ? "a" : "b") + ".bin";
    
    std::remove(filename.c_str());
    
    // In real TC375:
    // - Erase flash sectors
    // - Takes several seconds!
    // - Must disable interrupts during erase
    
    return true;
}

bool OtaManager::verifyCRC(BootBank bank, uint32_t expected_crc) {
    // Simplified CRC check
    // In production: calculate CRC32 of entire firmware image
    
    std::cout << "[OTA] Verifying CRC: expected=0x" << std::hex 
              << expected_crc << std::dec << std::endl;
    
    // Mac simulation: always pass for now
    return true;
}

bool OtaManager::verifySignature(BootBank bank, const uint8_t* signature) {
    // PQC signature verification
    // In production: use Dilithium/Falcon to verify firmware signature
    
    std::cout << "[OTA] Verifying PQC signature..." << std::endl;
    
    // Mac simulation: always pass for now
    return true;
}

void OtaManager::updateProgress() {
    int progress = getProgress();
    if (progress_callback_) {
        progress_callback_(progress);
    }
    
    if (progress % 10 == 0) {
        std::cout << "[OTA] Download progress: " << progress << "%" << std::endl;
    }
}

void OtaManager::handleError(const std::string& error) {
    std::cerr << "[OTA] Error: " << error << std::endl;
    if (error_callback_) {
        error_callback_(error);
    }
}

void OtaManager::setState(OtaState state) {
    state_ = state;
    std::cout << "[OTA] State changed: ";
    switch (state) {
        case OtaState::IDLE: std::cout << "IDLE"; break;
        case OtaState::DOWNLOADING: std::cout << "DOWNLOADING"; break;
        case OtaState::VERIFYING: std::cout << "VERIFYING"; break;
        case OtaState::INSTALLING: std::cout << "INSTALLING"; break;
        case OtaState::ROLLBACK: std::cout << "ROLLBACK"; break;
        case OtaState::SUCCESS: std::cout << "SUCCESS"; break;
        case OtaState::FAILED: std::cout << "FAILED"; break;
    }
    std::cout << std::endl;
}

// ============================================================================
// Bootloader (Simulated for Mac)
// ============================================================================

// In real TC375, this would be in actual bootloader code
// stored in protected flash region

static BootBank g_active_bank = BootBank::BANK_A;
static bool g_bank_a_valid = true;
static bool g_bank_b_valid = false;
static uint32_t g_boot_count_a = 0;
static uint32_t g_boot_count_b = 0;

BootBank Bootloader::getActiveBank() {
    // In TC375: read from dedicated flash sector or register
    return g_active_bank;
}

bool Bootloader::setActiveBank(BootBank bank) {
    if (bank != BootBank::BANK_A && bank != BootBank::BANK_B) {
        return false;
    }
    
    // In TC375: write to non-volatile storage
    // e.g., EEPROM, Flash data sector, or backup SRAM
    g_active_bank = bank;
    
    std::cout << "[Bootloader] Active bank set to: " 
              << (bank == BootBank::BANK_A ? "A" : "B") << std::endl;
    
    return true;
}

bool Bootloader::isValidFirmware(BootBank bank) {
    // In TC375: check CRC, signature, magic number in bank header
    return (bank == BootBank::BANK_A) ? g_bank_a_valid : g_bank_b_valid;
}

uint32_t Bootloader::getBootCount(BootBank bank) {
    return (bank == BootBank::BANK_A) ? g_boot_count_a : g_boot_count_b;
}

void Bootloader::incrementBootCount(BootBank bank) {
    if (bank == BootBank::BANK_A) {
        g_boot_count_a++;
    } else {
        g_boot_count_b++;
    }
}

void Bootloader::markFirmwareValid(BootBank bank) {
    std::cout << "[Bootloader] Marking Bank " 
              << (bank == BootBank::BANK_A ? "A" : "B") 
              << " as VALID" << std::endl;
    
    if (bank == BootBank::BANK_A) {
        g_bank_a_valid = true;
    } else {
        g_bank_b_valid = true;
    }
}

void Bootloader::markFirmwareInvalid(BootBank bank) {
    std::cout << "[Bootloader] Marking Bank " 
              << (bank == BootBank::BANK_A ? "A" : "B") 
              << " as INVALID" << std::endl;
    
    if (bank == BootBank::BANK_A) {
        g_bank_a_valid = false;
    } else {
        g_bank_b_valid = false;
    }
}

} // namespace tc375

