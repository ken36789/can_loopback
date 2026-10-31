/**
  ******************************************************************************
  * @file    can_display.c
  * @brief   在 STM32H750B-DK 的 4.3" LCD (480x272) 上顯示 CAN 收發資料。
  *
  *          使用 ST 韌體包裡的 BSP LCD 驅動 (Drivers/BSP/STM32H750B-DK) 和
  *          UTIL_LCD 文字繪圖函式 (Utilities/lcd)，不需要 TouchGFX。
  *
  *          和 ST 範例不同的兩個地方：
  *          1. 畫面緩衝區放在晶片內部的 AXI SRAM，不用外部 SDRAM。
  *          2. LTDC 的像素時脈 (PLL3) 在這裡自己設定，並確認 PLL 輸入來源是 HSE。
  ******************************************************************************
  */
#include "can_display.h"
#include <stdio.h>
#include <string.h>
#include "main.h"

/* 用相對路徑引用 BSP 和 UTIL_LCD，不需要修改專案的 include 路徑設定 */
#include "../../Drivers/BSP/STM32H750B-DK/stm32h750b_discovery_lcd.h"
#include "../../Utilities/lcd/stm32_lcd.h"

#define SCREEN_W   480U
#define SCREEN_H   272U

/* 畫面緩衝區：RGB565，每個像素 2 bytes，共 480 x 272 x 2 = 255 KB。
 * 放在內部 AXI SRAM (連結器的 RAM_D1)。
 * stm32h750b_discovery_conf.h 的 LCD_LAYER_0_ADDRESS 指向這個陣列。 */
uint16_t lcd_framebuffer[SCREEN_W * SCREEN_H] __attribute__((aligned(64)));

/* ---- 顏色 (ARGB8888，UTIL_LCD 會自動轉成 RGB565) ---- */
#define COLOR_BG      UTIL_LCD_COLOR_BLACK
#define COLOR_TITLE   UTIL_LCD_COLOR_DARKBLUE
#define COLOR_TEXT    UTIL_LCD_COLOR_WHITE
#define COLOR_DIM     UTIL_LCD_COLOR_GRAY
#define COLOR_LINE    UTIL_LCD_COLOR_DARKGRAY
#define COLOR_TX      UTIL_LCD_COLOR_ORANGE
#define COLOR_RX      UTIL_LCD_COLOR_GREEN
#define COLOR_ERR     UTIL_LCD_COLOR_RED

/* ---- 版面位置 (像素) ---- */
#define X_LABEL       12U     /* "TX" / "RX" 標籤 */
#define X_VALUE       60U     /* ID 和資料 */
#define X_COUNT       300U    /* 計數 */
#define Y_SUBTITLE    38U
#define Y_LINE_1      58U
#define Y_TX          62U
#define Y_TX_DATA     90U     /* 資料兩行 (Font20)：byte 0~7、byte 8~15 */
#define Y_LINE_2      138U
#define Y_RX          142U
#define Y_RX_DATA     170U
#define Y_LINE_3      218U
#define Y_STATUS      224U
#define Y_FOOTER      254U
#define DATA_ROW_H    22U     /* 兩行資料的間距 */

static int      lcd_ready = 0;

/* 在指定位置以指定字型、顏色畫一行字 (靠左) */
static void text_at(uint32_t x, uint32_t y, sFONT *font,
                    uint32_t fg, uint32_t bg, const char *s)
{
  UTIL_LCD_SetFont(font);
  UTIL_LCD_SetTextColor(fg);
  UTIL_LCD_SetBackColor(bg);
  UTIL_LCD_DisplayStringAt(x, y, (uint8_t *)s, LEFT_MODE);
}

/* 置中畫一行字 */
static void text_center(uint32_t y, sFONT *font,
                        uint32_t fg, uint32_t bg, const char *s)
{
  UTIL_LCD_SetFont(font);
  UTIL_LCD_SetTextColor(fg);
  UTIL_LCD_SetBackColor(bg);
  UTIL_LCD_DisplayStringAt(0, y, (uint8_t *)s, CENTER_MODE);
}

/* 把最多 8 個位元組排成 "00 01 02 ..." (固定 23 個字元，空位補空白蓋掉舊字) */
static void format_bytes(char *out, size_t size, const uint8_t *d, uint32_t len)
{
  size_t pos = 0;
  for (uint32_t i = 0; (i < 8U) && (pos < size); i++)
  {
    if (i < len)
      pos += (size_t)snprintf(out + pos, size - pos, i ? " %02X" : "%02X", d[i]);
    else
      pos += (size_t)snprintf(out + pos, size - pos, i ? "   " : "  ");
  }
}

/* 狀態列 (固定 30 字寬，蓋掉上一次的內容) */
static void show_status(const char *msg, uint32_t color)
{
  char buf[40];
  snprintf(buf, sizeof(buf), "%-30.30s", msg);
  text_at(X_LABEL, Y_STATUS, &Font20, color, COLOR_BG, buf);
}

/* 資料分兩行顯示：第一行 byte 0~7，第二行 byte 8~15 */
static void show_data(uint32_t y, const uint8_t *d, uint32_t len)
{
  char buf[40];

  format_bytes(buf, sizeof(buf), d, (len > 8U) ? 8U : len);
  text_at(X_VALUE, y, &Font20, COLOR_TEXT, COLOR_BG, buf);
  format_bytes(buf, sizeof(buf), d + 8, (len > 8U) ? (len - 8U) : 0U);
  text_at(X_VALUE, y + DATA_ROW_H, &Font20, COLOR_TEXT, COLOR_BG, buf);
}

static void draw_layout(void)
{
  UTIL_LCD_Clear(COLOR_BG);

  /* 標題列 */
  UTIL_LCD_FillRect(0, 0, SCREEN_W, 34, COLOR_TITLE);
  text_center(5, &Font24, COLOR_TEXT, COLOR_TITLE, "FDCAN1 Loopback Monitor");
  text_center(Y_SUBTITLE, &Font16, COLOR_DIM, COLOR_BG, "Loopback  CAN FD  1M/5M BRS  29-bit ID");

  /* 分隔線 */
  UTIL_LCD_DrawHLine(0, Y_LINE_1, SCREEN_W, COLOR_LINE);
  UTIL_LCD_DrawHLine(0, Y_LINE_2, SCREEN_W, COLOR_LINE);
  UTIL_LCD_DrawHLine(0, Y_LINE_3, SCREEN_W, COLOR_LINE);

  /* TX / RX 區塊 */
  text_at(X_LABEL, Y_TX, &Font24, COLOR_TX, COLOR_BG, "TX");
  text_at(X_VALUE, Y_TX, &Font24, COLOR_DIM, COLOR_BG, "ID --");
  text_at(X_LABEL, Y_RX, &Font24, COLOR_RX, COLOR_BG, "RX");
  text_at(X_VALUE, Y_RX, &Font24, COLOR_DIM, COLOR_BG, "ID --");

  show_status("Waiting for data...", COLOR_DIM);
  text_at(X_LABEL, Y_FOOTER, &Font16, COLOR_DIM, COLOR_BG, "TX->RX latency: --");
}

int32_t CAN_Display_Init(void)
{
  int32_t ret = BSP_LCD_InitEx(0, LCD_ORIENTATION_LANDSCAPE,
                               LCD_PIXEL_FORMAT_RGB565, SCREEN_W, SCREEN_H);
  if (ret != BSP_ERROR_NONE)
  {
    return ret;
  }

  UTIL_LCD_SetFuncDriver(&LCD_Driver);
  UTIL_LCD_SetLayer(0);
  draw_layout();
  lcd_ready = 1;
  return 0;
}

void CAN_Display_ShowTx(uint32_t id, const uint8_t *data, uint32_t len,
                        uint32_t count, int ok)
{
  char buf[40];

  if (!lcd_ready) return;
  if (len > CAN_DISPLAY_MAX_BYTES) len = CAN_DISPLAY_MAX_BYTES;

  snprintf(buf, sizeof(buf), "ID 0x%08lX", (unsigned long)id);
  text_at(X_VALUE, Y_TX, &Font24, COLOR_TEXT, COLOR_BG, buf);
  snprintf(buf, sizeof(buf), "count %-6lu", (unsigned long)count);
  text_at(X_COUNT, Y_TX + 4U, &Font20, COLOR_DIM, COLOR_BG, buf);
  show_data(Y_TX_DATA, data, len);

  if (!ok)
  {
    show_status("TX FAIL: queue full", COLOR_ERR);
  }
}

void CAN_Display_ShowRx(uint32_t id, const uint8_t *data, uint32_t len,
                        uint32_t count)
{
  char buf[40];

  if (!lcd_ready) return;
  if (len > CAN_DISPLAY_MAX_BYTES) len = CAN_DISPLAY_MAX_BYTES;

  snprintf(buf, sizeof(buf), "ID 0x%08lX", (unsigned long)id);
  text_at(X_VALUE, Y_RX, &Font24, COLOR_TEXT, COLOR_BG, buf);
  snprintf(buf, sizeof(buf), "count %-6lu", (unsigned long)count);
  text_at(X_COUNT, Y_RX + 4U, &Font20, COLOR_DIM, COLOR_BG, buf);
  show_data(Y_RX_DATA, data, len);
}

void CAN_Display_ShowCheck(const char *label, int good, uint32_t seq,
                           uint32_t errors, uint32_t lost)
{
  char buf[40];

  if (!lcd_ready) return;

  snprintf(buf, sizeof(buf), "%s #%lu  err %lu  lost %lu", label,
           (unsigned long)seq, (unsigned long)errors, (unsigned long)lost);
  show_status(buf, good ? COLOR_RX : COLOR_ERR);
}

void CAN_Display_ShowLatency(uint32_t last_ns, uint32_t min_ns,
                             uint32_t avg_ns, uint32_t max_ns)
{
  char line[48];
  char buf[48];

  if (!lcd_ready) return;

  /* 以 us 顯示 (四捨五入)。Font16 從 X_LABEL 起一行最多 42 字，固定 42 字寬蓋掉舊字 */
  snprintf(line, sizeof(line), "TX->RX %luus  min %lu  avg %lu  max %lu",
           (unsigned long)((last_ns + 500U) / 1000U), (unsigned long)((min_ns + 500U) / 1000U),
           (unsigned long)((avg_ns + 500U) / 1000U), (unsigned long)((max_ns + 500U) / 1000U));
  snprintf(buf, sizeof(buf), "%-42.42s", line);
  text_at(X_LABEL, Y_FOOTER, &Font16, COLOR_TEXT, COLOR_BG, buf);
}

/**
  * @brief  LTDC 像素時脈設定 (覆寫 BSP 裡的 __weak 版本)。
  *         BSP 原本的版本假設 PLL 輸入已經是 25 MHz 的 HSE，也沒有設定 PLL3RGE。
  *         PLL 輸入來源由三組 PLL 共用：SystemClock_Config 開 PLL1 時已經選 HSE；
  *         如果 PLL 都還沒開 (來源停在開機預設的 HSI 64 MHz)，這裡先切到 HSE。
  *         25 MHz / M(5) = 5 MHz -> x N(160) = 800 MHz -> / R(83) = 9.64 MHz
  */
HAL_StatusTypeDef MX_LTDC_ClockConfig(LTDC_HandleTypeDef *hltdc)
{
  RCC_PeriphCLKInitTypeDef clk = {0};
  (void)hltdc;

  /* PLL 的輸入來源只有在所有 PLL 都關閉時才能修改 */
  if ((RCC->CR & (RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON)) == 0U)
  {
    __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  }
  if (__HAL_RCC_GET_PLL_OSCSOURCE() != RCC_PLLSOURCE_HSE)
  {
    return HAL_ERROR;   /* PLL 已經由其他時脈驅動，需要改這裡的 M/N/R */
  }

  clk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  clk.PLL3.PLL3M      = 5;
  clk.PLL3.PLL3N      = 160;
  clk.PLL3.PLL3P      = 2;
  clk.PLL3.PLL3Q      = 2;
  clk.PLL3.PLL3R      = 83;
  clk.PLL3.PLL3RGE    = RCC_PLL3VCIRANGE_2;   /* PLL 輸入 4~8 MHz */
  clk.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  clk.PLL3.PLL3FRACN  = 0;
  return HAL_RCCEx_PeriphCLKConfig(&clk);
}
