/**
  ******************************************************************************
  * @file    can_app.c
  * @brief   CAN loopback 測試流程 (說明見 can_app.h)
  ******************************************************************************
  */
#include "can_app.h"
#include <stdio.h>
#include "can_config.h"
#include "can_comm.h"
#include "can_verify.h"
#include "can_latency.h"
#include "can_fault.h"
#include "can_display.h"

/* 把 ns 拆成 "整數 us" 和 "小數兩位"，給 printf 的 "%lu.%02lu" 用 (newlib-nano 預設不支援 %f) */
#define US_PARTS(ns)  (unsigned long)((ns) / 1000U), (unsigned long)(((ns) % 1000U) / 10U)

static int      ready = 0;
static uint32_t last_tx_ms = 0;
static uint32_t last_tx_stamp = 0;   /* 最後一包送出時的 CPU 週期計數 */
static uint32_t tx_count = 0;
static uint32_t rx_count = 0;
static uint8_t  tx_data[CAN_APP_DATA_LEN];

/* 產生一包 (序號 + 亂數)，依測試設定製造錯誤，然後送出 */
static void send_one(void)
{
  uint32_t seq = CAN_Verify_PrepareTx(tx_data);   /* 填入序號 + 亂數資料，並記下 TX 內容 */
  CAN_FaultAction fault = CAN_Fault_Apply(seq, tx_data, CAN_APP_DATA_LEN);

  if (fault == CAN_FAULT_DROP)
  {
    CAN_Verify_CommitTx();                         /* 序號用掉但不送 */
    printf("TEST: drop seq=%lu\r\n", seq);
    return;
  }
  if (fault == CAN_FAULT_CORRUPT)
  {
    const CAN_FaultStats *fs = CAN_Fault_GetStats();
    printf("TEST: corrupt seq=%lu byte %lu bit %lu\r\n", seq, fs->last_index, fs->last_bit);
  }

  if (CAN_Comm_Send(tx_data, &last_tx_stamp) == HAL_OK)
  {
    CAN_Verify_CommitTx();                         /* 確定放進佇列，序號才往下走 */
    tx_count++;
    printf("TX seq=%lu id=0x%08lX\r\n", seq, (unsigned long)CAN_APP_ID);
    CAN_Display_ShowTx(CAN_APP_ID, tx_data, CAN_APP_DATA_LEN, tx_count, 1);
  }
  else
  {
    printf("TX fail, free=%lu, PSR=0x%08lX\r\n", CAN_Comm_TxFreeLevel(), CAN_Comm_ProtocolStatus());
    CAN_Display_ShowTx(CAN_APP_ID, tx_data, CAN_APP_DATA_LEN, tx_count, 0);
  }
}

/* 印出比對結果和累計統計，並更新 LCD 狀態列 */
static void report_check(CAN_CheckResult chk)
{
  const CAN_VerifyStats *st = CAN_Verify_GetStats();
  uint32_t tec, rec;

  CAN_Comm_ErrorCounters(&tec, &rec);   /* 硬體錯誤計數，loopback 正常應該一直是 0 */
  printf("CHECK %s seq=%lu  ok=%lu lost=%lu dup=%lu err=%lu  TEC=%lu REC=%lu\r\n",
         CAN_Verify_ResultName(chk), st->last_seq, st->ok, st->lost, st->dup, st->errors, tec, rec);
  if (chk == CAN_CHECK_DATA_ERROR)
  {
    printf("  first bad byte %lu: TX %02X, RX %02X\r\n", st->bad_index, st->bad_tx, st->bad_rx);
  }
  if (CAN_Comm_RxOverflow() != 0U)
  {
    printf("  RX queue overflow: %lu frames dropped\r\n", CAN_Comm_RxOverflow());
  }
#if CAN_TEST_INJECT
  const CAN_FaultStats *fs = CAN_Fault_GetStats();
  printf("  injected: drop=%lu corrupt=%lu (should equal lost / err)\r\n", fs->dropped, fs->corrupted);
#endif

  CAN_Display_ShowCheck(CAN_Verify_ResultName(chk), chk == CAN_CHECK_OK,
                        st->last_seq, st->errors, st->lost);
}

/* 印出延遲統計，並更新 LCD 最下面一行 */
static void report_latency(void)
{
  const CAN_LatencyStats *lt = CAN_Latency_GetStats();

  printf("TX->RX %lu.%02lu us (min %lu.%02lu / avg %lu.%02lu / max %lu.%02lu)\r\n",
         US_PARTS(lt->last_ns), US_PARTS(lt->min_ns), US_PARTS(lt->avg_ns), US_PARTS(lt->max_ns));
  CAN_Display_ShowLatency(lt->last_ns, lt->min_ns, lt->avg_ns, lt->max_ns);
}

/* 收到一包：印出內容、和 TX 比對、統計延遲 */
static void handle_rx(const CAN_Frame *f)
{
  rx_count++;
  printf("RX id=0x%08lX data:", f->id);
  for (uint32_t i = 0; i < f->len; i++) printf(" %02X", f->data[i]);
  printf("\r\n");
  CAN_Display_ShowRx(f->id, f->data, f->len, rx_count);

  report_check(CAN_Verify_CheckRx(f->id, f->data, f->len));

  CAN_Latency_Record(last_tx_stamp, f->timestamp);
  report_latency();
}

void CAN_App_Init(FDCAN_HandleTypeDef *hfdcan)
{
  CAN_Latency_Init();   /* 要比 CAN 先開始，時間戳記才有效 */
  CAN_Verify_Init(CAN_APP_ID, CAN_APP_DATA_LEN);

  if (CAN_Comm_Init(hfdcan, CAN_APP_ID, CAN_APP_DATA_LEN, CAN_APP_BRS) != HAL_OK)
  {
    printf("CAN init failed (check CAN_APP_DATA_LEN in can_config.h)\r\n");
    return;
  }
  printf("CAN loopback start\r\n");

  int32_t lcdStatus = CAN_Display_Init();   /* 初始化螢幕並畫出版面 */
  if (lcdStatus != 0)
    printf("LCD init failed (BSP error %ld)\r\n", lcdStatus);
  else
    printf("LCD ready\r\n");

  ready = 1;
}

void CAN_App_Process(void)
{
  CAN_Frame frame;

  if (!ready) return;

  if (HAL_GetTick() - last_tx_ms >= CAN_APP_TX_PERIOD_MS)
  {
    last_tx_ms = HAL_GetTick();
    send_one();
  }
  while (CAN_Comm_Receive(&frame))
  {
    handle_rx(&frame);
  }
}
