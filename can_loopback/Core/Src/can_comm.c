/**
  ******************************************************************************
  * @file    can_comm.c
  * @brief   FDCAN 收發 (說明見 can_comm.h)
  ******************************************************************************
  */
#include "can_comm.h"
#include "can_latency.h"

static FDCAN_HandleTypeDef  *can = NULL;
static FDCAN_TxHeaderTypeDef tx_header;

/* 接收環形緩衝區：中斷寫入 (head)，主迴圈讀出 (tail)。
 * 索引一直往上加，用 head - tail 算出裡面有幾包，所以 8 格都能用滿。 */
static CAN_Frame         rx_queue[CAN_COMM_RX_QUEUE];
static volatile uint32_t rx_head = 0;
static volatile uint32_t rx_tail = 0;
static volatile uint32_t rx_overflow = 0;

/* DLC 代碼 -> 位元組數 (CAN FD 的 9~15 代表 12~64) */
static const uint8_t dlc_bytes[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/* 位元組數 -> DLC 代碼；不是合法長度回傳 0xFF。
 * 這版 HAL 的 FDCAN_DLC_BYTES_x 就是 DLC 代碼本身 (例如 FDCAN_DLC_BYTES_16 = 0xA) */
static uint32_t bytes_to_dlc(uint32_t len)
{
  for (uint32_t dlc = 0; dlc < 16U; dlc++)
  {
    if (dlc_bytes[dlc] == len) return dlc;
  }
  return 0xFFU;
}

HAL_StatusTypeDef CAN_Comm_Init(FDCAN_HandleTypeDef *hfdcan, uint32_t id, uint32_t len, int brs)
{
  FDCAN_FilterTypeDef filter = {0};
  uint32_t dlc = bytes_to_dlc(len);

  can = hfdcan;
  if (dlc == 0xFFU) return HAL_ERROR;

  filter.IdType       = FDCAN_EXTENDED_ID;   /* 29-bit ID 的過濾器 */
  filter.FilterIndex  = 0;
  filter.FilterType   = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1    = 0x000;
  filter.FilterID2    = 0x000;               /* mask 全 0 = 全部接收 */
  if (HAL_FDCAN_ConfigFilter(can, &filter) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_ActivateNotification(can, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;

  tx_header.Identifier          = id;
  tx_header.IdType              = FDCAN_EXTENDED_ID;
  tx_header.TxFrameType         = FDCAN_DATA_FRAME;
  tx_header.DataLength          = dlc;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch       = brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
  tx_header.FDFormat            = FDCAN_FD_CAN;   /* 超過 8 bytes 必須用 CAN FD 格式 */
  tx_header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker       = 0;

  return HAL_FDCAN_Start(can);
}

HAL_StatusTypeDef CAN_Comm_Send(const uint8_t *data, uint32_t *timestamp)
{
  *timestamp = CAN_Latency_Now();   /* 延遲量測起點：放進佇列前一刻 */
  return HAL_FDCAN_AddMessageToTxFifoQ(can, &tx_header, data);
}

int CAN_Comm_Receive(CAN_Frame *frame)
{
  if (rx_tail == rx_head) return 0;

  *frame = rx_queue[rx_tail % CAN_COMM_RX_QUEUE];
  __DMB();   /* 先複製完，再讓出這一格給中斷 */
  rx_tail++;
  return 1;
}

/* 覆寫 HAL 的 __weak 版本：FIFO0 收到新訊框時由 HAL_FDCAN_IRQHandler 呼叫 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  uint32_t now = CAN_Latency_Now();   /* 一進中斷就先記時間，不把讀取資料的時間算進去 */
  FDCAN_RxHeaderTypeDef hdr;

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U) return;

  /* 硬體 FIFO 裡可能不只一包，全部讀完 */
  while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U)
  {
    if ((rx_head - rx_tail) >= CAN_COMM_RX_QUEUE)
    {
      /* 緩衝區滿了：照樣從硬體 FIFO 讀走 (不然會一直卡著)，但這包丟掉 */
      uint8_t discard[CAN_COMM_MAX_BYTES];
      HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, discard);
      rx_overflow++;
      continue;
    }

    CAN_Frame *f = &rx_queue[rx_head % CAN_COMM_RX_QUEUE];
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, f->data) != HAL_OK) break;
    f->id        = hdr.Identifier;
    f->len       = dlc_bytes[hdr.DataLength & 0xFU];
    f->timestamp = now;
    __DMB();   /* 先寫完資料，再讓主迴圈看到這一格 */
    rx_head++;
  }
}

uint32_t CAN_Comm_TxFreeLevel(void)
{
  return HAL_FDCAN_GetTxFifoFreeLevel(can);
}

uint32_t CAN_Comm_ProtocolStatus(void)
{
  return can->Instance->PSR;
}

void CAN_Comm_ErrorCounters(uint32_t *tec, uint32_t *rec)
{
  FDCAN_ErrorCountersTypeDef ec;

  HAL_FDCAN_GetErrorCounters(can, &ec);
  *tec = ec.TxErrorCnt;
  *rec = ec.RxErrorCnt;
}

uint32_t CAN_Comm_RxOverflow(void)
{
  return rx_overflow;
}
