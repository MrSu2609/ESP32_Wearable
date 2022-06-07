/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <Queue.h>
#include "esp_bt_device.h"

#include "BLE_function.h"
/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define bleServerName "FPT_IoTS_BTempHRate01-%C%C%C%C"    //Max 26 character!
#define SERVICE_UUID "7bde7b9d-547e-4703-9785-ceedeeb2863e"
#define TX_CHARACTERISTIC_UUID "9d45a73a-b19f-4739-8339-ecad527b4455"
#define RX_CHARACTERISTIC_UUID "9a847af6-300b-4966-b32b-0c47a7b2c418"

#define MAX_PACKET_SIZE   90
#define SL_START_CHAR     0x24

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
/* Enumerated list of states for receive state machine */
typedef enum {
    E_STATE_WAIT_START,
    E_STATE_WAIT_SIZE,
    E_STATE_WAIT_STATUS,
    E_STATE_WAIT_ID,
    E_STATE_WAIT_SEQ_MSB,
    E_STATE_WAIT_SEQ_LSB,
    E_STATE_WAIT_STAM_MSB,
    E_STATE_WAIT_STAM_LSB,
    E_STATE_WAIT_DATA,
    E_STATE_WAIT_CRC,
    E_STATE_WAIT_CRC1,
}APP_teDataState;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static uint8_t APP_u8CalculateCRC(uint8_t Size, uint8_t Staus, uint8_t ID, uint16_t SeqID, uint16_t TimeStamp, uint8_t *pMsgData);
static bool BLE_vRxCharParser(uint8_t u8RxChar);

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
BLEServer* pServer = NULL;
BLECharacteristic* TxCharacteristic = NULL;
BLECharacteristic* RxCharacteristic = NULL;
bool deviceConnected = false;

tsQueue asBLE_TxQueue;
tsQueue asBLE_RxQueue;

static uint8_t MsgTxSize = 0;

static uint8_t  MsgRxDataBuffer[MAX_PACKET_SIZE];
static uint8_t  MsgRxSize;
static uint8_t  MsgRxStatus;
static uint8_t  MsgRxID;
static uint16_t MsgSeqID;
static uint16_t MsgTimeStam;
static uint8_t  MsgCRC;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};

class CharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      for (int i = 0; i < value.length(); i++)
      {
        bQueue_Write(&asBLE_RxQueue, value[i]);
      }
    }
  }
};

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

void BLE_getMAC(uint8_t *DeviceID)
{
  const uint8_t* macAddress = esp_bt_dev_get_address();
  char charMAC[4];
  sprintf(charMAC, "%02X", (int)macAddress[4]);
  sprintf(charMAC+2, "%02X", (int)macAddress[5]);
  for(int i=0;i<4;i++)
    DeviceID[i] = charMAC[i];
}

void BLE_Init(uint8_t *DeviceID, uint8_t *pu8TxBuffer, uint32_t u32TxBufferLen, uint8_t *pu8RxBuffer, uint32_t u32RxBufferLen)
{
  char Servername[] = bleServerName;
  sprintf(Servername, bleServerName, DeviceID[0], DeviceID[1], DeviceID[2], DeviceID[3]);
  BLEDevice::init(Servername);
  BLEDevice::setMTU(105);
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // Create a BLE Characteristic
  TxCharacteristic = pService->createCharacteristic(
                      TX_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  RxCharacteristic = pService->createCharacteristic(
                      RX_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  RxCharacteristic->setCallbacks(new CharacteristicCallbacks());
  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  /* Initialise Tx & Rx Queue's */
	vQueue_Init(&asBLE_TxQueue, pu8TxBuffer, u32TxBufferLen);
	vQueue_Init(&asBLE_RxQueue, pu8RxBuffer, u32RxBufferLen);
}

void BLE_Advertising(bool Start)
{
  if(Start)
    BLEDevice::startAdvertising();
  else{
    BLEDevice::stopAdvertising();
    //BLEDevice::deinit(false);
  }
}

bool BLE_isConnected(void)
{
  return deviceConnected;
}

void BLE_sendMsg(void)
{
  static APP_teDataState eTxState = E_STATE_WAIT_START;
	static uint8_t u8Bytes;
  static uint8_t *Msg;

	uint8_t u8TxByte;
  if(bQueue_Read(&asBLE_TxQueue, &u8TxByte))
  {
    switch(eTxState)
    {
      case E_STATE_WAIT_START:
        if(u8TxByte == SL_START_CHAR)
        {
          u8Bytes = 0;
          eTxState = E_STATE_WAIT_SIZE;
        }
        break;
      case E_STATE_WAIT_SIZE:
        MsgTxSize = u8TxByte;
        if(MsgTxSize > ((int)MAX_PACKET_SIZE + 10)){
          eTxState = E_STATE_WAIT_START;
        }
        else{
          Msg = new uint8_t[(int)MsgTxSize + 2];
          Msg[u8Bytes++] = SL_START_CHAR;
          Msg[u8Bytes++] = MsgTxSize;
          eTxState = E_STATE_WAIT_DATA;
        }
        break;
      case E_STATE_WAIT_DATA:
        Msg[u8Bytes++] = u8TxByte;
        if(u8Bytes >= MsgTxSize)
          eTxState = E_STATE_WAIT_CRC;
        break;
      case E_STATE_WAIT_CRC:
        Msg[(int)MsgTxSize] = u8TxByte;
        eTxState = E_STATE_WAIT_CRC1;
        break;
      case E_STATE_WAIT_CRC1:
        Msg[(int)MsgTxSize + 1] = u8TxByte;
        //Send data
        TxCharacteristic->setValue(Msg, (int)MsgTxSize + 2);
        TxCharacteristic->notify();
        delete[] Msg;
        eTxState = E_STATE_WAIT_START;
        break;
    }
  }
}

void BLE_configMsg(uint8_t Size, uint8_t Staus, uint8_t ID, uint16_t SeqID, uint16_t TimeStamp, uint8_t *pMsgData)
{
  int n;
  uint8_t b8CRC = APP_u8CalculateCRC(Size, Staus, ID, SeqID, TimeStamp, pMsgData);

  bQueue_Write(&asBLE_TxQueue, SL_START_CHAR);
  bQueue_Write(&asBLE_TxQueue, Size);
  bQueue_Write(&asBLE_TxQueue, Staus);
  bQueue_Write(&asBLE_TxQueue, ID);
  bQueue_Write(&asBLE_TxQueue, (SeqID >> 8) & 0xff);
  bQueue_Write(&asBLE_TxQueue, (SeqID >> 0) & 0xff);
  bQueue_Write(&asBLE_TxQueue, (TimeStamp >> 8) & 0xff);
  bQueue_Write(&asBLE_TxQueue, (TimeStamp >> 0) & 0xff);

  for(n = 0; n < (Size - 8); n++){
    bQueue_Write(&asBLE_TxQueue, pMsgData[n]);
  }
  bQueue_Write(&asBLE_TxQueue, 0x00);
  bQueue_Write(&asBLE_TxQueue, b8CRC);
}

bool BLE_RxDataProcess(void)
{
  uint8_t u8RxByte;
	while(bQueue_Read(&asBLE_RxQueue, &u8RxByte))
	{
    if(BLE_vRxCharParser(u8RxByte))
      return true;
  }
  return false;
}

uint8_t BLE_getMsgID(void)
{
  return MsgRxID;
}

uint8_t BLE_getMsgSize(void)
{
  return MsgRxSize;
}

uint8_t* BLE_getMsgData(void)
{
  return MsgRxDataBuffer;
}
/****************************************************************************/
/***              Local Function			                                     **/
/****************************************************************************/

static bool BLE_vRxCharParser(uint8_t u8RxChar)
{
  static APP_teDataState eRxState = E_STATE_WAIT_START;
  static uint8_t u8Bytes;

  switch (eRxState)
  {
    case E_STATE_WAIT_START:
      if(u8RxChar == SL_START_CHAR){
        u8Bytes = 0;
        eRxState = E_STATE_WAIT_SIZE;
      }
      break;
    case E_STATE_WAIT_SIZE:
      if((u8RxChar < 8) || (u8RxChar > MAX_PACKET_SIZE)){
        eRxState = E_STATE_WAIT_START;
      }
      else{
        MsgRxSize = u8RxChar;
        eRxState = E_STATE_WAIT_STATUS;
      }
      break;
    case E_STATE_WAIT_STATUS:
      MsgRxStatus = u8RxChar;
      eRxState = E_STATE_WAIT_ID;
      break;
    case E_STATE_WAIT_ID:
      MsgRxID = u8RxChar;
      eRxState = E_STATE_WAIT_SEQ_MSB;
      break;
    case E_STATE_WAIT_SEQ_MSB:
      MsgSeqID = u8RxChar*256;
      eRxState = E_STATE_WAIT_SEQ_LSB;
      break;
    case E_STATE_WAIT_SEQ_LSB:
      MsgSeqID += u8RxChar;
      eRxState = E_STATE_WAIT_STAM_MSB;
      break;
    case E_STATE_WAIT_STAM_MSB:
      MsgTimeStam = u8RxChar*256;
      eRxState = E_STATE_WAIT_STAM_LSB;
      break;
    case E_STATE_WAIT_STAM_LSB:
      MsgTimeStam += u8RxChar;
      if(MsgRxSize < 8)
        eRxState = E_STATE_WAIT_START;
      else if(MsgRxSize == 8){
        eRxState = E_STATE_WAIT_CRC;
      } 
      break;
    case E_STATE_WAIT_DATA:
      if(u8Bytes < (MsgRxSize - 8)){
        MsgRxDataBuffer[u8Bytes++] = u8RxChar;
      }
      else{
        eRxState = E_STATE_WAIT_CRC;
      }
      break;
    case E_STATE_WAIT_CRC:
      //MsgCRC = u8RxChar*256;
      eRxState = E_STATE_WAIT_CRC1;
      break;
    case E_STATE_WAIT_CRC1:
      eRxState = E_STATE_WAIT_START;
      if(u8RxChar == APP_u8CalculateCRC(MsgRxSize, MsgRxStatus, MsgRxID, MsgSeqID, MsgTimeStam, MsgRxDataBuffer))
        return true;
      else{
        Serial.println("BAD CRC!");
      }
        break;
  }
  return false;
}

static uint8_t APP_u8CalculateCRC(uint8_t Size, uint8_t Staus, uint8_t ID, uint16_t SeqID, uint16_t TimeStamp, uint8_t *pMsgData)
{
  int n;
  uint8_t u8CRC;

  u8CRC = SL_START_CHAR & 0xff;
  u8CRC ^= Size;
  u8CRC ^= Staus;
  u8CRC ^= ID;
  u8CRC ^= (SeqID >> 8) & 0xff;
  u8CRC ^= (SeqID >> 0) & 0xff;
  u8CRC ^= (TimeStamp >> 8) & 0xff;
  u8CRC ^= (TimeStamp >> 0) & 0xff;

  if(Size > 19){
    for (n = 0; n < 11; n++) {
      u8CRC ^= pMsgData[n];
    }
  }
  else{
    for (n = 0; n < (Size - 8); n++) {
      u8CRC ^= pMsgData[n];
    }
  }
  return (u8CRC);
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
