#pragma once

#include <string>
#include <cstdint>
#include <functional>

namespace tc375 {

// OTA States
enum class OtaState {
    IDLE,
    DOWNLOADING,
    VERIFYING,
    INSTALLING,
    ROLLBACK,
    SUCCESS,
    FAILED
};

// Boot Bank (A/B partition)
enum class BootBank {
    BANK_A = 0,
    BANK_B = 1,
    INVALID = 0xFF
};

// Firmware metadata
struct FirmwareMetadata {
    uint32_t version;
    uint32_t size;
    uint32_t crc32;
    uint8_t signature[256];  // PQC signature
    std::string build_date;
    
    bool isValid() const;
    std::string toString() const;
};

// OTA Manager with A/B partition support
class OtaManager {
public:
    OtaManager();
    ~OtaManager() = default;

    // OTA Operations
    bool startDownload(uint32_t firmware_size, const FirmwareMetadata& metadata);
    bool writeBlock(uint32_t offset, const uint8_t* data, size_t length);
    bool verify();
    bool install();
    bool rollback();
    
    // Bank management
    BootBank getCurrentBank() const { return current_bank_; }
    BootBank getTargetBank() const;
    bool switchBank(BootBank bank);
    
    // Status
    OtaState getState() const { return state_; }
    int getProgress() const;  // 0-100%
    std::string getStatusReport() const;
    
    // Callbacks
    using ProgressCallback = std::function<void(int percentage)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }

private:
    OtaState state_;
    BootBank current_bank_;
    
    // Download state
    uint32_t target_size_;
    uint32_t bytes_written_;
    FirmwareMetadata target_metadata_;
    std::string temp_file_path_;
    
    // Callbacks
    ProgressCallback progress_callback_;
    ErrorCallback error_callback_;
    
    // Bank metadata
    struct BankMetadata {
        bool valid;
        FirmwareMetadata firmware;
        uint32_t boot_count;
        uint32_t last_boot_timestamp;
    };
    BankMetadata bank_a_meta_;
    BankMetadata bank_b_meta_;
    
    // Private methods
    bool saveBankMetadata(BootBank bank, const BankMetadata& meta);
    bool loadBankMetadata(BootBank bank, BankMetadata& meta);
    bool writeToFlash(BootBank bank, uint32_t offset, const uint8_t* data, size_t length);
    bool eraseBank(BootBank bank);
    bool verifyCRC(BootBank bank, uint32_t expected_crc);
    bool verifySignature(BootBank bank, const uint8_t* signature);
    
    void updateProgress();
    void handleError(const std::string& error);
    void setState(OtaState state);
};

// Bootloader interface (simulated for Mac, real for TC375)
class Bootloader {
public:
    static BootBank getActiveBank();
    static bool setActiveBank(BootBank bank);
    static bool isValidFirmware(BootBank bank);
    static uint32_t getBootCount(BootBank bank);
    static void incrementBootCount(BootBank bank);
    static void markFirmwareValid(BootBank bank);
    static void markFirmwareInvalid(BootBank bank);
};

} // namespace tc375

