/**
  ******************************************************************************
  * @file    can_latency.c
  * @brief   TX -> RX 延遲量測 (說明見 can_latency.h)
  ******************************************************************************
  */
#include "can_latency.h"
#include <string.h>
#include "main.h"   /* DWT、CoreDebug、SystemCoreClock */

static CAN_LatencyStats stats;
static uint32_t         min_cyc;
static uint32_t         max_cyc;
static uint64_t         sum_cyc;

static uint32_t cycles_to_ns(uint32_t cyc)
{
  return (uint32_t)(((uint64_t)cyc * 1000000000ULL) / SystemCoreClock);
}

void CAN_Latency_Init(void)
{
  /* DWT 週期計數器：每個 CPU 時脈 +1 (400 MHz -> 2.5 ns，約 10.7 秒繞回一次) */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR = 0xC5ACCE55;   /* Cortex-M7 要先解鎖 DWT 才能寫入 */
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  memset(&stats, 0, sizeof(stats));
  min_cyc = UINT32_MAX;
  max_cyc = 0;
  sum_cyc = 0;
}

uint32_t CAN_Latency_Now(void)
{
  return DWT->CYCCNT;
}

void CAN_Latency_Record(uint32_t tx_stamp, uint32_t rx_stamp)
{
  uint32_t cyc = rx_stamp - tx_stamp;   /* 無號數相減，計數器繞回也算得對 */

  if (cyc < min_cyc) min_cyc = cyc;
  if (cyc > max_cyc) max_cyc = cyc;
  sum_cyc += cyc;
  stats.count++;

  stats.last_ns = cycles_to_ns(cyc);
  stats.min_ns  = cycles_to_ns(min_cyc);
  stats.max_ns  = cycles_to_ns(max_cyc);
  stats.avg_ns  = cycles_to_ns((uint32_t)(sum_cyc / stats.count));
}

const CAN_LatencyStats *CAN_Latency_GetStats(void)
{
  return &stats;
}
