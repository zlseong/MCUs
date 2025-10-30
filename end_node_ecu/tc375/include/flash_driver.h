/**
 * TC375 Flash Driver
 * 
 * Infineon TC375 PFLASH/DFLASH 하드웨어 제어
 * 
 * Features:
 *   - Dual Bank Support (Region A/B 독립 Erase/Program)
 *   - Memory Protection (실행 중인 Region은 하드웨어 보호)
 *   - ECC Support (Error Correction Code)
 */

#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Flash Type
typedef enum {
    FLASH_TYPE_PFLASH = 0,  // Program Flash (6 MB)
    FLASH_TYPE_DFLASH       // Data Flash (384 KB)
} FlashType;

// Flash Sector Size
#define PFLASH_SECTOR_SIZE      (16 * 1024)     // 16 KB
#define DFLASH_SECTOR_SIZE      (4 * 1024)      // 4 KB

/**
 * @brief Flash 초기화
 * @return true if success
 */
bool Flash_Init(void);

/**
 * @brief Flash Erase (Sector 단위)
 * 
 * @param address 시작 주소 (sector-aligned)
 * @param size 크기 (sector-aligned)
 * @return true if success
 * 
 * Note:
 *   - 실행 중인 Region은 Erase 불가 (Hardware Exception)
 *   - TC375 MPU가 자동으로 보호
 */
bool Flash_Erase(uint32_t address, uint32_t size);

/**
 * @brief Flash Write (Page 단위)
 * 
 * @param address 시작 주소 (page-aligned)
 * @param data 데이터
 * @param size 크기
 * @return true if success
 * 
 * Note:
 *   - Erase 후에만 Program 가능
 *   - 실행 중인 Region은 Program 불가
 */
bool Flash_Write(uint32_t address, const uint8_t* data, uint32_t size);

/**
 * @brief Flash Read
 * 
 * @param address 시작 주소
 * @param data 버퍼
 * @param size 크기
 * @return true if success
 */
bool Flash_Read(uint32_t address, uint8_t* data, uint32_t size);

/**
 * @brief Flash Verify (Written data 검증)
 * 
 * @param address 시작 주소
 * @param expected_data 예상 데이터
 * @param size 크기
 * @return true if match
 */
bool Flash_Verify(uint32_t address, const uint8_t* expected_data, uint32_t size);

/**
 * @brief 현재 실행 중인 Region 확인
 * @return 0 (Region A) or 1 (Region B)
 */
uint8_t Flash_GetActiveRegion(void);

/**
 * @brief Flash 상태 확인
 * @return true if ready
 */
bool Flash_IsReady(void);

/**
 * @brief Flash 에러 코드 조회
 * @return 에러 코드 (0 = No error)
 */
uint32_t Flash_GetError(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_DRIVER_H */

