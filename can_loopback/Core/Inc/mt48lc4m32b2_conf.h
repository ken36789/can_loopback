/* SDRAM 晶片設定 (由 mt48lc4m32b2_conf_template.h 複製)。目前沒有使用 SDRAM，只是讓標頭檔能編譯。 */
#ifndef MT48LC4M32B2_CONF_H
#define MT48LC4M32B2_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32h7xx_hal.h"

#define REFRESH_COUNT                    ((uint32_t)0x0603)   /* SDRAM refresh counter */
#define MT48LC4M32B2_TIMEOUT             ((uint32_t)0xFFFF)

#ifdef __cplusplus
}
#endif
#endif /* MT48LC4M32B2_CONF_H */
