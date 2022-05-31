#include <Arduino.h>
#include <BLE_function.h>
#include <sensor_function.h>

#include <IO_function.h>
#include <Kernel_IO_function.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp32-mqtt.h"

#include <BLEDevice.h>
#include "esp_bt_device.h"


#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)

#define MAX_BRIGHTNESS 255
#define bufferLength 100

uint8_t value[54] = "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghij";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool WorkMode = false;
bool start = false;

/* Temp varialble */
double temp_obj;
/* Heartbeat varialble */
int beatAvg = 0;
int AvgMax = 0;
bool agvValueFlag = false;
/* SPO2 varialble */
int oxygen;
int SPO2Max = 0;
bool spo2ValueFlag = false;

/*  IO Variable */
extern IO_Struct pLED1, pLED1, pLED3;
structIO_Manage_Output strLED_RD, strLED_GR, strLED_BL;
extern IO_Struct pBUT_1, pBUT_2;
structIO_Button strIO_Button_Value, strOld_IO_Button_Value;

void OLED_Display(double temp, int heartBeat, int32_t SPO2, bool mode);

void task_Temp(void *parameter) {
  for(;;) {
    temp_obj = mlx_getTemp();
    if(WorkMode)
    {
      OLED_Display(temp_obj, beatAvg, oxygen, true);
    }
    else
    {
      if(start)
      {
        if(!agvValueFlag){
          if(AvgMax <= beatAvg)
            AvgMax = beatAvg;
          else
            agvValueFlag = true;
        }
        if(!spo2ValueFlag){
          if(SPO2Max <= oxygen)
            SPO2Max = oxygen;
          else
            spo2ValueFlag = true;
        }
        if(agvValueFlag&&spo2ValueFlag)
        {
          agvValueFlag = false;
          spo2ValueFlag = false;
          start = false;
        }
        else{
          OLED_Display(temp_obj, AvgMax, SPO2Max, false);
        }
      }
      else
      {
        OLED_Display(temp_obj, AvgMax, SPO2Max, true);
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void task_MAX3010x(void *parameter) {
  for(;;) {
    MAX30105_getValue(oxygen, beatAvg);
  }
}
void task_IO(void *parameter)
{
  for(;;) {
    vIO_Output(&strLED_GR, &pLED1);
	  vIO_Output(&strLED_RD, &pLED2);
	  vIO_Output(&strLED_BL, &pLED3);

    /*** Get BT Value  ***/
    vGetIOButtonValue(eButton1, pBUT_1.read(), &strOld_IO_Button_Value, &strIO_Button_Value);
    vGetIOButtonValue(eButton2, pBUT_2.read(), &strOld_IO_Button_Value, &strIO_Button_Value);

    if(strOld_IO_Button_Value.bButtonState[eButton1] != strIO_Button_Value.bButtonState[eButton1])
    {
      strOld_IO_Button_Value.bButtonState[eButton1] = strIO_Button_Value.bButtonState[eButton1];
      strIO_Button_Value.bFlagNewButton = eTRUE;
    }
    if(strOld_IO_Button_Value.bButtonState[eButton2] != strIO_Button_Value.bButtonState[eButton2])
    {
      strOld_IO_Button_Value.bButtonState[eButton2] = strIO_Button_Value.bButtonState[eButton2];
      strIO_Button_Value.bFlagNewButton = eTRUE;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

String getMACinString() {
  const uint8_t* macAddress = esp_bt_dev_get_address();
  char charMAC[18];

  sprintf(charMAC, "%02X", (int)macAddress[0]);
  charMAC[2] = ':';
  sprintf(charMAC+3, "%02X", (int)macAddress[1]);
  charMAC[5] = ':';

  sprintf(charMAC+6, "%02X", (int)macAddress[2]);
  charMAC[8] = ':';
  sprintf(charMAC+9, "%02X", (int)macAddress[3]);
  charMAC[11] = ':';

  sprintf(charMAC+12, "%02X", (int)macAddress[4]);
  charMAC[14] = ':';
  sprintf(charMAC+15, "%02X", (int)macAddress[5]);

  return (String)charMAC;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  /* BLE setup */
  Serial.println("[DEBUG]: SYSTEM INIT!");
  Serial.println("[DEBUG]: LED INIT!");
  LED1_Init(&pLED1);
  LED2_Init(&pLED2);
  LED3_Init(&pLED3);

  Serial.println("[DEBUG]: BUTTON INIT!");
  strIO_Button_Value.bFlagNewButton =  eFALSE;
  BUTTON1_Init(&pBUT_1);
  strIO_Button_Value.bButtonState[eButton1] = eButtonRelease;
  strOld_IO_Button_Value.bButtonState[eButton1] = eButtonRelease;

  BUTTON2_Init(&pBUT_2);
  strIO_Button_Value.bButtonState[eButton2] = eButtonRelease;
  strOld_IO_Button_Value.bButtonState[eButton2] = eButtonRelease;
  
  /*Serial.println("[DEBUG]: SENSOR & LCD INIT!");
  sensor_setUp();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
 
  display.clearDisplay();
  display.setCursor(25,15);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("FPT Wearable");
  display.setCursor(25,35);
  display.setTextSize(1);
  display.print("Press Start BT!");
  display.display();*/
  /* BLE setup */
  /*Serial.println("BLE setup!");
  BLE_Setup();
  delay(1000);
  String myMACString = getMACinString();
  Serial.println("My MAC address = " + myMACString);*/
  /* Cloud setup */
  setupCloudIoT();
  //wifi_Setup();
  //xTaskCreate(task_Temp,"Task 1",8192,NULL,2,NULL);
  //xTaskCreate(task_MAX3010x,"Task 2",8192,NULL,1,NULL);
  xTaskCreate(task_IO,"Task 3",8192,NULL,1,NULL);
}

unsigned long lastMillis = 0;
void loop() {
  mqtt->loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!mqttClient->connected()) {
    connect();
  }

  // TODO: replace with your code
  // publish a message roughly every second.
  if (millis() - lastMillis > 60000) {
    lastMillis = millis();
    //publishTelemetry(mqttClient, "/sensors", getDefaultSensor());
    publishTelemetry(getDefaultSensor());
  }
}

void OLED_Display(double temp, int heartBeat, int32_t SPO2, bool mode)
{
  display.clearDisplay();
  display.setCursor(0,10);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Temperature: ");
  display.print(temp);
  display.print((char)247);
  display.println("C");
  //display.display();
  display.setCursor(0,20);
  if(mode)
  {
    
    display.print("BPM = ");
    display.println(heartBeat);
    display.print("SPO2 = ");
    display.println(SPO2);
  }
  else{
    display.print("Processing...");
  }
  display.display(); 
}