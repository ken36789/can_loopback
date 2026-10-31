/**
  ******************************************************************************
  * @file    can_fault.c
  * @brief   測試用故障注入 (說明見 can_fault.h)
  ******************************************************************************
  */
#include "can_fault.h"
#include "can_config.h"
#include "can_verify.h"   /* CAN_Verify_Random：和 TX 亂數資料共用同一個亂數序列 */

static CAN_FaultStats stats;

CAN_FaultAction CAN_Fault_Apply(uint32_t seq, uint8_t *data, uint32_t len)
{
#if CAN_TEST_INJECT == 1
  int drop    = (seq % 10U == 9U);
  int corrupt = (seq % 15U == 14U);
#elif CAN_TEST_INJECT == 2
  int drop    = (CAN_Verify_Random() % 100U) < CAN_TEST_DROP_PCT;
  int corrupt = (CAN_Verify_Random() % 100U) < CAN_TEST_CORRUPT_PCT;
  (void)seq;
#else
  (void)seq;
  (void)data;
  (void)len;
  return CAN_FAULT_NONE;
#endif

#if CAN_TEST_INJECT
  if (drop)
  {
    stats.dropped++;
    return CAN_FAULT_DROP;
  }
  if (corrupt && (len > 4U))
  {
    /* 只改 byte 4 之後；改到 byte 0~3 會變成序號錯誤，結果會顯示 NO TX 或 LOST */
    stats.last_index = 4U + CAN_Verify_Random() % (len - 4U);
    stats.last_bit   = CAN_Verify_Random() % 8U;
    data[stats.last_index] ^= (uint8_t)(1U << stats.last_bit);
    stats.corrupted++;
    return CAN_FAULT_CORRUPT;
  }
  return CAN_FAULT_NONE;
#endif
}

const CAN_FaultStats *CAN_Fault_GetStats(void)
{
  return &stats;
}
