/**
  ******************************************************************************
  * @file    can_comm.h
  * @brief   FDCAN 收發：過濾器、啟動、送出，以及用中斷把收到的訊框放進環形緩衝區
  *
  *          FDCAN 本身的初始化 (位元時序、模式、Message RAM) 在 main.c 的 MX_FDCAN1_Init。
  ******************************************************************************
  */
#ifndef CAN_COMM_H
#define CAN_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"   /* FDCAN_HandleTypeDef、HAL_StatusTypeDef */

#define CAN_COMM_MAX_BYTES  64U   /* CAN FD 一包最多 64 bytes */
#define CAN_COMM_RX_QUEUE   8U    /* 接收緩衝區能放幾包 */

typedef struct
{
  uint32_t id;
  uint32_t len;                         /* 位元組數 (已由 DLC 代碼換算) */
  uint8_t  data[CAN_COMM_MAX_BYTES];
  uint32_t timestamp;                   /* 收到時的 CPU 週期計數 (中斷一進來就記) */
} CAN_Frame;

/* 設定過濾器 (29-bit ID 全收)、開啟接收中斷、準備 TX 標頭並啟動 FDCAN。
 * len 不是 CAN FD 合法長度，或 HAL 設定失敗時回傳 HAL_ERROR。 */
HAL_StatusTypeDef CAN_Comm_Init(FDCAN_HandleTypeDef *hfdcan, uint32_t id, uint32_t len, int brs);

/* 送出一包 (長度為 Init 設定的 len)。timestamp 回傳放進佇列前一刻的 CPU 週期計數 */
HAL_StatusTypeDef CAN_Comm_Send(const uint8_t *data, uint32_t *timestamp);

/* 從接收緩衝區取出一包：有資料回傳 1，沒有回傳 0 */
int CAN_Comm_Receive(CAN_Frame *frame);

/* 除錯資訊 */
uint32_t CAN_Comm_TxFreeLevel(void);
uint32_t CAN_Comm_ProtocolStatus(void);                   /* PSR 暫存器 */
void     CAN_Comm_ErrorCounters(uint32_t *tec, uint32_t *rec);
uint32_t CAN_Comm_RxOverflow(void);                       /* 接收緩衝區滿了而丟掉的包數 */

#ifdef __cplusplus
}
#endif

#endif /* CAN_COMM_H */
