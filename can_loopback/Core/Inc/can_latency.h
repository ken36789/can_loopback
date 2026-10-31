/**
  ******************************************************************************
  * @file    can_latency.h
  * @brief   TX -> RX 延遲量測：用 DWT 週期計數器計時，統計最小 / 平均 / 最大
  ******************************************************************************
  */
#ifndef CAN_LATENCY_H
#define CAN_LATENCY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint32_t count;     /* 已統計幾包 */
  uint32_t last_ns;   /* 最後一包 */
  uint32_t min_ns;
  uint32_t avg_ns;
  uint32_t max_ns;
} CAN_LatencyStats;

/* 開啟 DWT 週期計數器並清除統計。要在 CAN 開始收發前呼叫 */
void CAN_Latency_Init(void);

/* 目前的 CPU 週期計數，當作時間戳記 */
uint32_t CAN_Latency_Now(void);

/* 用一組 TX / RX 時間戳記更新統計 */
void CAN_Latency_Record(uint32_t tx_stamp, uint32_t rx_stamp);

const CAN_LatencyStats *CAN_Latency_GetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_LATENCY_H */
