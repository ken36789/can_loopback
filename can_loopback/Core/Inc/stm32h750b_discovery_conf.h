/**
  ******************************************************************************
  * @file    stm32h750b_discovery_conf.h
  * @brief   BSP 設定檔 (由 stm32h750b_discovery_conf_template.h 複製修改)
  *
  *          和範本不同的地方：畫面緩衝區放在晶片內部 RAM (can_display.c 的
  *          lcd_framebuffer)，所以定義 DATA_IN_ExtSDRAM 讓 BSP 跳過外部 SDRAM 初始化。
  ******************************************************************************
  */
#ifndef STM32H750B_DK_CONF_H
#define STM32H750B_DK_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32h7xx_hal.h"

/* COM define */
#define USE_COM_LOG                         0U
#define USE_BSP_COM_FEATURE                 0U

/* Touch controllers defines */
#define USE_FT5336_TS_CTRL                  1U

/* LCD 畫面緩衝區：放在內部 AXI SRAM，見 can_display.c */
extern uint16_t lcd_framebuffer[];
#define LCD_LAYER_0_ADDRESS                 ((uint32_t)lcd_framebuffer)
#define LCD_LAYER_1_ADDRESS                 0xD0200000U   /* 沒有使用 */
#define DATA_IN_ExtSDRAM                    /* 不初始化外部 SDRAM */

/* Audio codecs defines */
#define USE_AUDIO_CODEC_WM8994              1U

/* Default Audio IN internal buffer size */
#define DEFAULT_AUDIO_IN_BUFFER_SIZE        64U
/* TS supported features defines */
#define USE_TS_GESTURE                      1U
#define USE_TS_MULTI_TOUCH                  1U

/* Default TS touch number */
#define TS_TOUCH_NBR                        2U

/* IRQ priorities */
#define BSP_SDRAM_IT_PRIORITY               15U
#define BSP_BUTTON_USER_IT_PRIORITY         15U
#define BSP_AUDIO_OUT_IT_PRIORITY           14U
#define BSP_AUDIO_IN_IT_PRIORITY            15U
#define BSP_SD_IT_PRIORITY                  14U
#define BSP_SD_RX_IT_PRIORITY               14U
#define BSP_SD_TX_IT_PRIORITY               15U
#define BSP_TS_IT_PRIORITY                  15U

#ifdef __cplusplus
}
#endif

#endif /* STM32H750B_DK_CONF_H */
