/* 編譯 ST 的 UTIL_LCD 文字/繪圖函式和字型。
 * Utilities 資料夾不在專案的編譯路徑內：stm32_lcd.c 會直接 #include 所有字型 .c，
 * 若字型檔再單獨編譯一次會重複定義，所以只在這裡 include 一次。 */
#include "../../Utilities/lcd/stm32_lcd.c"
