/**
  ******************************************************************************
  * @file    can_verify.c
  * @brief   比對 TX 和 RX 的內容，找出漏包、重複和資料錯誤 (說明見 can_verify.h)
  ******************************************************************************
  */
#include "can_verify.h"
#include <string.h>

typedef struct
{
  uint32_t seq;
  uint8_t  data[CAN_VERIFY_MAX_BYTES];
  uint8_t  valid;
} TxRecord;

static TxRecord        tx_history[CAN_VERIFY_HISTORY];
static CAN_VerifyStats stats;
static uint32_t        expect_id;
static uint32_t        expect_len;
static uint32_t        tx_seq;      /* 下一包 TX 的序號 */
static uint32_t        rx_expect;   /* 下一包 RX 預期的序號 */
static uint32_t        rng_state;   /* 亂數狀態，不能是 0 */

uint32_t CAN_Verify_Random(void)
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static uint32_t read_seq(const uint8_t *d)
{
  return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

void CAN_Verify_Init(uint32_t id, uint32_t len)
{
  expect_id  = id;
  expect_len = (len > CAN_VERIFY_MAX_BYTES) ? CAN_VERIFY_MAX_BYTES : len;
  tx_seq     = 0;
  rx_expect  = 0;
  rng_state  = 0x2545F491U;
  memset(tx_history, 0, sizeof(tx_history));
  memset(&stats, 0, sizeof(stats));
}

uint32_t CAN_Verify_PrepareTx(uint8_t *data)
{
  TxRecord *rec = &tx_history[tx_seq % CAN_VERIFY_HISTORY];

  /* byte 0~3：序號；之後：亂數，各種 bit 組合都測得到 */
  for (uint32_t i = 0; i < expect_len; i++)
  {
    data[i] = (i < 4U) ? (uint8_t)(tx_seq >> (8U * i)) : (uint8_t)CAN_Verify_Random();
  }
  rec->seq = tx_seq;
  memcpy(rec->data, data, expect_len);
  rec->valid = 1;
  return tx_seq;
}

void CAN_Verify_CommitTx(void)
{
  tx_seq++;
}

CAN_CheckResult CAN_Verify_CheckRx(uint32_t id, const uint8_t *data, uint32_t len)
{
  CAN_CheckResult result = CAN_CHECK_OK;

  if ((id != expect_id) || (len != expect_len))
  {
    stats.errors++;
    return CAN_CHECK_BAD_FORMAT;
  }

  uint32_t seq = read_seq(data);
  stats.last_seq = seq;

  /* 序號必須是已經送出、而且還留在 TX 紀錄裡的；否則可能是序號本身被改壞了 */
  const TxRecord *rec = &tx_history[seq % CAN_VERIFY_HISTORY];
  if (((int32_t)(seq - tx_seq) >= 0) || !rec->valid || (rec->seq != seq))
  {
    stats.errors++;
    return CAN_CHECK_NO_TX;
  }

  /* 序號追蹤：跳號 = 中間漏包，變小 = 重複或順序錯亂 (有號差值，繞回也判斷得對) */
  int32_t diff = (int32_t)(seq - rx_expect);
  if (diff < 0)
  {
    stats.dup++;
    result = CAN_CHECK_DUPLICATE;
  }
  else
  {
    if (diff > 0)
    {
      stats.lost += (uint32_t)diff;
      result = CAN_CHECK_GAP;
    }
    rx_expect = seq + 1U;
  }

  /* 內容比對：記下第一個不同的 byte */
  for (uint32_t i = 0; i < len; i++)
  {
    if (data[i] != rec->data[i])
    {
      stats.errors++;
      stats.bad_index = i;
      stats.bad_tx    = rec->data[i];
      stats.bad_rx    = data[i];
      return CAN_CHECK_DATA_ERROR;
    }
  }

  if (result != CAN_CHECK_DUPLICATE)
  {
    stats.ok++;
  }
  return result;
}

const CAN_VerifyStats *CAN_Verify_GetStats(void)
{
  return &stats;
}

const char *CAN_Verify_ResultName(CAN_CheckResult r)
{
  switch (r)
  {
    case CAN_CHECK_OK:         return "OK";
    case CAN_CHECK_GAP:        return "LOST";
    case CAN_CHECK_DUPLICATE:  return "DUP";
    case CAN_CHECK_BAD_FORMAT: return "FORMAT";
    case CAN_CHECK_NO_TX:      return "NO TX";
    case CAN_CHECK_DATA_ERROR: return "DATA";
    default:                   return "?";
  }
}
