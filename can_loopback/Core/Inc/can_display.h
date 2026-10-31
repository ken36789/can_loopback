/**
  ******************************************************************************
  * @file    can_display.h
  * @brief   在 STM32H750B-DK 的 4.3" LCD 上顯示 CAN 收發資料
  ******************************************************************************
  */
#ifndef CAN_DISPLAY_H
#define CAN_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 畫面最多顯示的資料位元組數 (分兩行，每行 8 bytes)，超過的部分不顯示 */
#define CAN_DISPLAY_MAX_BYTES  16U

/* 初始化 LCD 並畫出固定版面。成功回傳 0，失敗回傳 BSP 錯誤碼（負數）。 */
int32_t CAN_Display_Init(void);

/* 更新 TX 區塊。ok = 1 表示成功放進發送佇列，0 表示失敗。 */
void CAN_Display_ShowTx(uint32_t id, const uint8_t *data, uint32_t len,
                        uint32_t count, int ok);

/* 更新 RX 區塊，並和最後一包 TX 比對內容。 */
void CAN_Display_ShowRx(uint32_t id, const uint8_t *data, uint32_t len,
                        uint32_t count);

/* 更新最下面一行的 TX -> RX 延遲 (單位 ns)：這一包、最小、平均、最大 */
void CAN_Display_ShowLatency(uint32_t last_ns, uint32_t min_ns,
                             uint32_t avg_ns, uint32_t max_ns);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DISPLAY_H */
