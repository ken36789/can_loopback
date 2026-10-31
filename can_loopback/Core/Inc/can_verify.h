/**
  ******************************************************************************
  * @file    can_verify.h
  * @brief   比對 TX 和 RX 的內容，找出漏包、重複和資料錯誤
  *
  *          每包資料的 byte 0~3 放序號 (little-endian)，其餘放亂數測試資料。
  *          送出前記下 TX 內容，收到後用序號找回對應的那包 TX，逐 byte 比對。
  ******************************************************************************
  */
#ifndef CAN_VERIFY_H
#define CAN_VERIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CAN_VERIFY_MAX_BYTES  64U   /* 每包最多比對幾個 byte (CAN FD 上限) */
#define CAN_VERIFY_HISTORY    8U    /* 記住最近幾包 TX，和 TX FIFO 深度一樣 */

typedef enum
{
  CAN_CHECK_OK = 0,       /* 序號連續，內容和 TX 一致 */
  CAN_CHECK_GAP,          /* 這包內容正確，但前面有漏包 (序號跳號) */
  CAN_CHECK_DUPLICATE,    /* 序號比預期小：重複收到或順序錯亂 */
  CAN_CHECK_BAD_FORMAT,   /* ID 或長度和 TX 不同 */
  CAN_CHECK_NO_TX,        /* 找不到這個序號的 TX 紀錄 (序號本身可能被改壞) */
  CAN_CHECK_DATA_ERROR    /* 內容和 TX 不同 */
} CAN_CheckResult;

typedef struct
{
  uint32_t ok;          /* 內容正確的包數 */
  uint32_t lost;        /* 漏掉的包數 (由序號跳號推算) */
  uint32_t dup;         /* 重複或順序錯亂 */
  uint32_t errors;      /* 格式錯誤 + 找不到 TX + 內容錯誤 */
  uint32_t last_seq;    /* 最後收到的序號 */
  uint32_t bad_index;   /* 最後一次內容錯誤：第幾個 byte */
  uint8_t  bad_tx;      /*                    TX 送的值 */
  uint8_t  bad_rx;      /*                    RX 收到的值 */
} CAN_VerifyStats;

/* 設定預期的 ID 和長度 (至少 4 bytes 才放得下序號)，並清除統計 */
void CAN_Verify_Init(uint32_t id, uint32_t len);

/* 填好一包資料 (序號 + 亂數資料) 並記下 TX 內容，回傳這包的序號。
 * 真的放進發送佇列後要呼叫 CAN_Verify_CommitTx()，序號才會往下走。 */
uint32_t CAN_Verify_PrepareTx(uint8_t *data);
void     CAN_Verify_CommitTx(void);

/* 檢查收到的一包並更新統計 */
CAN_CheckResult CAN_Verify_CheckRx(uint32_t id, const uint8_t *data, uint32_t len);

const CAN_VerifyStats *CAN_Verify_GetStats(void);
const char *CAN_Verify_ResultName(CAN_CheckResult r);

/* 測試用亂數 (xorshift32)。CAN_Verify_Init 會重設種子，所以每次開機的序列都一樣，方便重現問題 */
uint32_t CAN_Verify_Random(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_VERIFY_H */
