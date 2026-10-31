/**
  ******************************************************************************
  * @file    can_config.h
  * @brief   CAN 測試程式的可調整參數，集中在這裡修改
  *
  *          位元時序 (鮑率)、loopback 模式、Message RAM 大小在 main.c 的 MX_FDCAN1_Init。
  ******************************************************************************
  */
#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

/* ---- 送出的訊框 ---- */
#define CAN_APP_ID            0x12345678U   /* 29-bit ID，最大 0x1FFFFFFF */
#define CAN_APP_DATA_LEN      16U           /* 每包位元組數，必須是 CAN FD 合法長度：0~8、12、16、20、24、32、48、64。
                                               MX_FDCAN1_Init 的 TxElmtSize / RxFifo0ElmtSize 要放得下 */
#define CAN_APP_BRS           1             /* 1 = 資料段切換到 Data 位元率 (MX_FDCAN1_Init 的 FrameFormat 要是 FD_BRS) */
#define CAN_APP_TX_PERIOD_MS  1000U         /* 多久送一包 */

/* ---- 故意製造錯誤，確認 can_verify 抓得到 ---- */
/* 0 = 關閉；1 = 固定間隔 (每 10 包漏 1 包、每 15 包改壞 1 包)；2 = 隨機 */
#define CAN_TEST_INJECT       2
#define CAN_TEST_DROP_PCT     10U   /* 模式 2：每包有 10% 機率故意不送 (模擬漏包) */
#define CAN_TEST_CORRUPT_PCT  10U   /* 模式 2：每包有 10% 機率翻轉 1 個 bit (模擬資料錯誤) */

#endif /* CAN_CONFIG_H */
