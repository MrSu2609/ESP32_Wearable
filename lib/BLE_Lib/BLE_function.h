#ifndef _BLE_FUNCTION_H_
#define _BLE_FUNCTION_H_

#include <stdint.h>

/*	Commands Type List	*/
typedef enum {
	E_TEMP_DATA_ID        = 0x01,
	E_HRSPO2_DATA_ID      = 0X02,
	E_STATUS_DEVICE_ID    = 0x03,
    E_SSID_CFG_ID         = 0x04,
    E_PASS_CFG_ID         = 0x05,
    E_INTERVAL_CFG_ID     = 0x06,
    E_WORK_MODE_ID        = 0x07,
    E_URL_CFG_ID          = 0x08,
    E_DEVICE_DATA_ID      = 0x09,
    E_PRIVATE_KEY_ID      = 0x0A,
    E_ONESHOT_TEMP_ID     = 0x0B,
    E_ONESHOT_SPO2_ID     = 0x0C,
    E_ONESHOT_MODE_ID     = 0x0D,
    E_CONTINUOUS_MODE_ID  = 0x0E,
    E_PRODUCT_INFO_ID     = 0x21,
    E_RESTART_DEVICE_ID   = 0x22,
} Msg_teID_Type;

void BLE_getMAC(uint8_t *DeviceID);

void BLE_Init(uint8_t *DeviceID, uint8_t *pu8TxBuffer, uint32_t u32TxBufferLen, uint8_t *pu8RxBuffer, uint32_t u32RxBufferLen);

void BLE_Advertising(bool Start);

bool BLE_isConnected(void);

void BLE_sendMsg(void);

void BLE_configMsg(uint8_t Size, uint8_t Staus, uint8_t ID, uint16_t SeqID, uint16_t TimeStamp, uint8_t *pMsgData);

bool BLE_RxDataProcess(void);

uint8_t BLE_getMsgID(void);

uint8_t BLE_getMsgSize(void);

uint8_t* BLE_getMsgData(void);

#endif