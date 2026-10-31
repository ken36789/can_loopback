/* FT5336 觸控晶片設定 (由 ft5336_conf_template.h 複製)。LCD 初始化時 BSP 會讀它的 ID。 */
#ifndef FT5336_CONF_H
#define FT5336_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define FT5336_AUTO_CALIBRATION_ENABLED      0U
#define FT5336_MAX_X_LENGTH                  800U
#define FT5336_MAX_Y_LENGTH                  480U

#ifdef __cplusplus
}
#endif
#endif /* FT5336_CONF_H */
