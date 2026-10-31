/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "can_display.h"   // LCD 顯示 (Core/Src/can_display.c)
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CAN_DATA_LEN  16U   // 每包資料長度，需和 txHeader.DataLength (FDCAN_DLC_BYTES_16) 一致
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
// 把 ns 拆成 "整數 us" 和 "小數兩位"，給 printf 的 "%lu.%02lu" 用 (newlib-nano 預設不支援 %f)
#define US_PARTS(ns)  (unsigned long)((ns) / 1000U), (unsigned long)(((ns) % 1000U) / 10U)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

FDCAN_HandleTypeDef hfdcan1;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
FDCAN_TxHeaderTypeDef txHeader;
FDCAN_RxHeaderTypeDef rxHeader;
uint8_t txData[CAN_DATA_LEN];
uint8_t rxData[64];        // HAL 依收到的 DLC 寫入，CAN FD 最多 64 bytes
volatile uint8_t rxFlag = 0;
uint32_t txCount = 0, rxCount = 0;

// TX -> RX 延遲量測 (單位：CPU 週期，DWT 計數器)
volatile uint32_t txStartCyc = 0;     // 呼叫送出前的時間點
volatile uint32_t rxLatencyCyc = 0;   // RX 中斷裡算出的 TX -> RX 週期數
uint32_t latMinCyc = UINT32_MAX, latMaxCyc = 0;
uint64_t latSumCyc = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch)   // 讓 printf 輸出到 USART3
{
  HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

static uint32_t dlc_to_bytes(uint32_t dlc)   // DLC 代碼 -> 位元組數 (CAN FD 的 9~15 代表 12~64)
{
  static const uint8_t table[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
  return table[dlc & 0xFU];
}

static void cycle_counter_init(void)   // 開啟 DWT 週期計數器：每個 CPU 時脈 +1 (25 MHz -> 40 ns)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->LAR = 0xC5ACCE55;               // Cortex-M7 要先解鎖 DWT 才能寫入
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t cycles_to_ns(uint32_t cyc)
{
  return (uint32_t)(((uint64_t)cyc * 1000000000ULL) / SystemCoreClock);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  uint32_t now = DWT->CYCCNT;          // 一進中斷就先記時間，不把讀取資料的時間算進去
  if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
    HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rxHeader, rxData);
    rxLatencyCyc = now - txStartCyc;   // 無號數相減，計數器繞回也算得對
    rxFlag = 1;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  FDCAN_FilterTypeDef filter = {0};
  filter.IdType = FDCAN_EXTENDED_ID;   // 29-bit ID 的過濾器
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0x000;
  filter.FilterID2 = 0x000;          // mask 全 0 = 全部接收
  HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  cycle_counter_init();
  HAL_FDCAN_Start(&hfdcan1);

  txHeader.Identifier = 0x12345678;              // 29-bit ID，最大 0x1FFFFFFF
  txHeader.IdType = FDCAN_EXTENDED_ID;
  txHeader.TxFrameType = FDCAN_DATA_FRAME;
  txHeader.DataLength = FDCAN_DLC_BYTES_16;      // DLC 代碼 0xA = 16 bytes
  txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  txHeader.BitRateSwitch = FDCAN_BRS_ON;         // ID/ACK 用 1 Mbit/s，DLC~CRC 用 2.5 Mbit/s
  txHeader.FDFormat = FDCAN_FD_CAN;              // 超過 8 bytes 必須用 CAN FD 格式
  txHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  txHeader.MessageMarker = 0;
  printf("CAN loopback start\r\n");

  int32_t lcdStatus = CAN_Display_Init();   // 初始化螢幕並畫出版面
  if (lcdStatus != 0)
    printf("LCD init failed (BSP error %ld)\r\n", lcdStatus);
  else
    printf("LCD ready\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t lastTx = 0;
    static uint8_t counter = 0;
    if (HAL_GetTick() - lastTx >= 1000) {        // 每秒發一包
      lastTx = HAL_GetTick();
      for (uint32_t i = 0; i < CAN_DATA_LEN; i++) txData[i] = counter + i;
      counter++;
      txStartCyc = DWT->CYCCNT;                  // 延遲量測起點
      if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, txData) == HAL_OK) {
        txCount++;
        printf("TX id=0x%08lX\r\n", txHeader.Identifier);
        CAN_Display_ShowTx(txHeader.Identifier, txData, CAN_DATA_LEN, txCount, 1);
      } else {
        printf("TX fail, free=%lu, PSR=0x%08lX\r\n",
               HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1), hfdcan1.Instance->PSR);
        CAN_Display_ShowTx(txHeader.Identifier, txData, CAN_DATA_LEN, txCount, 0);
      }
    }
    if (rxFlag) {
      // 先把中斷收到的資料複製出來，避免印到一半被下一包蓋掉
      FDCAN_RxHeaderTypeDef hdr;
      uint8_t data[sizeof(rxData)];
      __disable_irq();
      hdr = rxHeader;
      memcpy(data, rxData, sizeof(data));
      uint32_t latCyc = rxLatencyCyc;
      rxFlag = 0;
      __enable_irq();

      uint32_t len = dlc_to_bytes(hdr.DataLength);  // CAN FD: DLC 是代碼，要查表換成位元組數
      rxCount++;
      printf("RX id=0x%08lX data:", hdr.Identifier);
      for (uint32_t i = 0; i < len; i++) printf(" %02X", data[i]);
      printf("\r\n");
      CAN_Display_ShowRx(hdr.Identifier, data, len, rxCount);

      // TX -> RX 延遲統計
      if (latCyc < latMinCyc) latMinCyc = latCyc;
      if (latCyc > latMaxCyc) latMaxCyc = latCyc;
      latSumCyc += latCyc;
      uint32_t latNs = cycles_to_ns(latCyc);
      uint32_t minNs = cycles_to_ns(latMinCyc);
      uint32_t maxNs = cycles_to_ns(latMaxCyc);
      uint32_t avgNs = cycles_to_ns((uint32_t)(latSumCyc / rxCount));
      printf("TX->RX %lu.%02lu us (min %lu.%02lu / avg %lu.%02lu / max %lu.%02lu)\r\n",
             US_PARTS(latNs), US_PARTS(minNs), US_PARTS(avgNs), US_PARTS(maxNs));
      CAN_Display_ShowLatency(latNs, minNs, avgNs, maxNs);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;      // CAN FD，資料段切換到較高位元率 (BRS)
  hfdcan1.Init.Mode = FDCAN_MODE_INTERNAL_LOOPBACK;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 1;     // 25 MHz / 1 = 25 MHz -> 1 tq = 40 ns
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 19;     // 1 + 19 + 5 = 25 tq = 1 us -> 1 Mbit/s
  hfdcan1.Init.NominalTimeSeg2 = 5;      // 取樣點 (1 + 19) / 25 = 80%
  hfdcan1.Init.DataPrescaler = 1;        // 資料段 (BRS 開啟時才使用)：1 tq = 40 ns
  hfdcan1.Init.DataSyncJumpWidth = 2;
  hfdcan1.Init.DataTimeSeg1 = 7;         // 1 + 7 + 2 = 10 tq = 400 ns -> 2.5 Mbit/s
  hfdcan1.Init.DataTimeSeg2 = 2;         // 取樣點 (1 + 7) / 10 = 80%
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 1;                      // 改用 29-bit ID 的過濾器
  hfdcan1.Init.RxFifo0ElmtsNbr = 8;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_16;  // Message RAM 每格要放得下 16 bytes
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 8;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_16;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
