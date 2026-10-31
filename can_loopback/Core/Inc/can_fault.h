/**
  ******************************************************************************
  * @file    can_fault.h
  * @brief   測試用：故意製造漏包或資料錯誤，確認 can_verify 抓得到
  *
  *          模式和機率在 can_config.h 的 CAN_TEST_INJECT 設定。
  ******************************************************************************
  */
#ifndef CAN_FAULT_H
#define CAN_FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  CAN_FAULT_NONE = 0,
  CAN_FAULT_DROP,       /* 這包不送 (模擬漏包) */
  CAN_FAULT_CORRUPT     /* 已翻轉 data 裡的 1 個 bit (模擬資料錯誤) */
} CAN_FaultAction;

typedef struct
{
  uint32_t dropped;     /* 累計故意不送的包數，應該等於 can_verify 的 lost */
  uint32_t corrupted;   /* 累計故意改壞的包數，應該等於 can_verify 的 errors */
  uint32_t last_index;  /* 最後一次改壞：第幾個 byte */
  uint32_t last_bit;    /*               第幾個 bit */
} CAN_FaultStats;

/* 決定這包要不要製造錯誤；回傳 CAN_FAULT_CORRUPT 時 data 已經被改過。
 * CAN_TEST_INJECT = 0 時一律回傳 CAN_FAULT_NONE。 */
CAN_FaultAction CAN_Fault_Apply(uint32_t seq, uint8_t *data, uint32_t len);

const CAN_FaultStats *CAN_Fault_GetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_FAULT_H */
