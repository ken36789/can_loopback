/**
  ******************************************************************************
  * @file    can_app.h
  * @brief   CAN loopback 測試流程：定時送出、收到後比對與量測延遲，結果輸出到 UART 和 LCD
  *
  *          各模組分工：
  *            can_config   可調整的參數
  *            can_comm     FDCAN 收發 (硬體層)
  *            can_verify   TX / RX 資料比對 (漏包、重複、資料錯誤)
  *            can_latency  TX -> RX 延遲計時與統計
  *            can_fault    測試用：故意製造錯誤
  *            can_display  LCD 顯示
  ******************************************************************************
  */
#ifndef CAN_APP_H
#define CAN_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"   /* FDCAN_HandleTypeDef */

/* 初始化所有 CAN 相關模組和 LCD。在 MX_FDCAN1_Init、MX_USART3_UART_Init 之後呼叫 */
void CAN_App_Init(FDCAN_HandleTypeDef *hfdcan);

/* 放在 main 的 while(1) 裡一直呼叫：時間到就送一包，有收到就處理 */
void CAN_App_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_APP_H */
