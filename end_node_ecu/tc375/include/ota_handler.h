/**
 * OTA Handler for TC375 Dual-Bank System
 * 
 * Application이 실행 중일 때 비활성 Region을 업데이트하는 핵심 모듈
 * 
 * Responsibilities:
 *   1. OTA 패키지 수신
 *   2. 비활성 Region Erase
 *   3. 비활성 Region Program
 *   4. Boot Config 업데이트
 * 
 * Note:
 *   - Bootloader는 검증만 담당
 *   - Application이 실제 Flash 작업 수행
 */

#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// OTA Buffer Size (DFLASH에 저장)
#define OTA_BUFFER_SIZE         (3 * 1024 * 1024)  // 3 MB (Bootloader + App)

// OTA State
typedef enum {
    OTA_IDLE = 0,           // 대기 중
    OTA_DOWNLOADING,        // 다운로드 중
    OTA_INSTALLING,         // 설치 중 (Flash 작업)
    OTA_COMPLETE,           // 완료 (Reboot 필요)
    OTA_FAILED              // 실패
} OTA_State;

/**
 * @brief OTA 초기화
 */
void ota_init(void);

/**
 * @brief OTA 시작
 * @param expected_size OTA 패키지 전체 크기 (bytes)
 * @return true if success
 */
bool ota_start(uint32_t expected_size);

/**
 * @brief OTA 데이터 수신 (Chunk 단위)
 * @param data 수신 데이터
 * @param size 데이터 크기
 * @return true if success
 */
bool ota_receive_chunk(const uint8_t* data, uint32_t size);

/**
 * @brief OTA 설치 (Flash Erase & Program)
 * 
 * 핵심 함수!
 * Application이 실행 중일 때, 비활성 Region을 업데이트합니다.
 * 
 * Process:
 *   1. 현재 Region 감지 (A or B)
 *   2. 대상 Region 계산 (현재가 A면 B, B면 A)
 *   3. 대상 Region Bootloader Erase & Program
 *   4. 대상 Region Application Erase & Program
 *   5. Boot Config 업데이트 (대상 Region 선택)
 * 
 * @return true if success
 */
bool ota_install(void);

/**
 * @brief OTA 상태 조회
 * @return 현재 OTA 상태
 */
OTA_State ota_get_state(void);

/**
 * @brief OTA 진행률 조회
 * @return 수신된 바이트 수
 */
uint32_t ota_get_progress(void);

/**
 * @brief OTA 취소
 */
void ota_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_HANDLER_H */

