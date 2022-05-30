#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <Button2.h>

#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

#define BUTTON_1            35
#define BUTTON_2            0

Button2 btn1, btn2;

unsigned long previousMillis = 0;
const long interval = 2000;

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

static BLEUUID meCoffeeServiceUUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID charUUID("0000ffe1-0000-1000-8000-00805f9b34fb");

static boolean doConnect = false;
static boolean connected = false;
static boolean brewing = false;
static unsigned long shotStarted = -1;

static BLEAdvertisedDevice* myDevice;
static BLERemoteCharacteristic* pRemoteCharacteristic;

static boolean warmupEnabled = true;
static boolean warmupRunning = false;
static int warmupTime;
static boolean warmupNeeded = false;

static boolean cleaning = false;
static int cleanReps = 5;
static int cleanTime = 10; // Seconds
static int cleanCount;
static boolean waiting = false;
static unsigned long waitStarted = -1;
static boolean rinsing = false;
static boolean rinsed = false;

static String currentShotTime = "";
static String currentTemperature = "";

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());

      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(meCoffeeServiceUUID)) {
        BLEDevice::getScan()->stop();
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnect = true;
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");
  
  tft.init();

  // Rotate screen to landscape mode
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK); // Clear Screen
  tft.setTextSize(3);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Scanning...",  tft.width() / 2, tft.height() / 2 );

  delay(2000);
  sleepDisplay();

  btn1.begin(BUTTON_1);
  btn1.setLongClickTime(2000);
  btn1.setClickHandler(singleClick);
  btn1.setLongClickDetectedHandler(longClickDetected);

  btn2.begin(BUTTON_2);
  btn2.setClickHandler(singleClick);
  
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    sleepDisplay();
    tft.fillScreen(TFT_BLACK); // Clear Screen
    tft.setTextSize(3);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Connected.",  tft.width() / 2, tft.height() / 2 );
    tft.setTextDatum(MR_DATUM);
    
    delay(2000);
    
    tft.setTextSize(5);
    sleepDisplay();
    
    currentShotTime = "";
    currentTemperature = "";
    if (!warmupEnabled) {
      drawShotTime("0s", TFT_LIGHTGREY);
    }
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    brewing = false;
    Serial.println("onDisconnect");
    
    tft.fillScreen(TFT_BLACK); // Clear Screen
    tft.setTextSize(3);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Disconnected.",  tft.width() / 2, tft.height() / 2 );
    
    delay(2000);
    sleepDisplay();  
  }
};

void enableWarmup() {
  warmupEnabled = true;
  Serial.println("Warmup enabled");
}

void disableWarmup() {
  warmupEnabled = false;
  warmupRunning = false;
  warmupNeeded = false;
  warmupTime = 0;
  drawShotTime("0s", TFT_LIGHTGREY);
  currentShotTime = "";
  Serial.println("Warmup disabled");
}

void initWarmup(float floatTemp) {
  if (floatTemp < 50) {
    warmupTime = 15 * 60; // 15 min
    warmupNeeded = true;
    warmupRunning = true;
  } else if (floatTemp < 70) {
    warmupTime = 10 * 60; // 10 min
    warmupNeeded = true;
    warmupRunning = true;
  } else if (floatTemp < 85) {
    warmupTime = 5 * 60; // 5 min
    warmupNeeded = true;
    warmupRunning = true;
  } else {
    warmupTime = 0 * 60; // 0 min
    disableWarmup();
  }
}

void updateWarmupTimer(int onTime) {
  if (warmupNeeded) {
    int warmupTimeDiff = warmupTime - onTime;
    if (warmupTimeDiff < 0) {
      disableWarmup();
    } else {
      uint16_t color = TFT_RED;
      char message[6];
      if ((onTime % 5) == 0) {
        int m = sprintf(message, "Warmup");
      } else {
        int warmupDiffMin = warmupTimeDiff / 60;
        int warmupDiffSec = warmupTimeDiff - (warmupDiffMin * 60);
        int m = sprintf(message, "%02dm%02ds", warmupDiffMin, warmupDiffSec);
      }
      drawShotTime(message, color);
    }
  }
}

void startCleaning() {
  cleaning = true;
  Serial.println("Cleaning mode started");
  drawShotTime("#Clean#", TFT_SKYBLUE);
  delay(3000);
  drawShotTime("-Flush-", TFT_SKYBLUE);
}

void stopCleaning() {
  cleanCount = 0;
  rinsing = false;
  cleaning = false;
  Serial.println("Cleaning mode stopped");
  drawShotTime("0s", TFT_LIGHTGREY);
}

void drawTemperature(String temperature, uint16_t color) {
  if (currentTemperature != temperature) {
    if (currentTemperature.length() > temperature.length()) {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString(currentTemperature, tft.width() - 7, tft.height() / 4 + 10 );      
    }
  
    currentTemperature = temperature;
    
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(currentTemperature, tft.width() - 7, tft.height() / 4 + 10 );
  }
}

void drawShotTime(String shotTime, uint16_t color) {
  if (currentShotTime != shotTime) {
    if (currentShotTime.length() > shotTime.length()) {
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString(" " + currentShotTime, tft.width() - 7, 3 * (tft.height() / 4) - 10 );
    }
  
    currentShotTime = shotTime;
    
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(" " + currentShotTime, tft.width() - 7, 3 * (tft.height() / 4) - 10 );
  }
}

void drawCleanTime(unsigned long startTime) {
  uint16_t color = TFT_SKYBLUE;
  char message[7];
  int cTime = (millis() - startTime);
  int m = sprintf(message, "x%d %3ds", cleanCount, cTime / 1000);
  //int m = sprintf(message, "%d/%d %2ds", cleanCount, cleanReps, cTime / 1000);
  if (cTime >= (cleanTime * 1000)) {
    color = TFT_GREEN;
    if (cTime >= ((cleanTime + 1) * 1000)) {
      if (waiting) {
        drawShotTime("-Flush-", TFT_SKYBLUE);
        waiting = false;
        return;
      } else {
        drawShotTime("-Stop-", TFT_SKYBLUE);
        return;
      }
    }
  }
  drawShotTime(message, color);
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    String sData = (char*)pData;

    if (sData.startsWith("tmp")) {
      int onTime, reqTemp, curTemp;

      sscanf((char*)pData, "tmp %d %d %d", &onTime, &reqTemp, &curTemp);

      // Set acceptable temperature diff
      float accDiff = 50; // 50 = 0.5C
      uint16_t color = (curTemp > (reqTemp - accDiff) && curTemp < (reqTemp + accDiff)) ? TFT_GREEN : TFT_ORANGE;
      
      float floatTemp = float(curTemp) / 100;
      
      drawTemperature(String(floatTemp) + "C", color);

      if (warmupEnabled) {
        if (!warmupRunning) {
          initWarmup(floatTemp);
        } else {
          updateWarmupTimer(onTime);
        }
      }
      
      if (cleaning && waiting) {
        drawCleanTime(waitStarted);
      }
    } else if (sData.startsWith("sht")) {
      int i, ms;
      uint16_t color;

      sscanf((char*)pData, "sht %d %d", &i, &ms);
      if (ms == 0) {
        brewing = true;
        
        shotStarted = millis();
        color = TFT_YELLOW;
      } else {
        brewing = false;
        color = TFT_LIGHTGREY;
      }

      if (cleaning) {
        color = TFT_SKYBLUE;
        if (rinsing) {
          if (!brewing) {
            drawShotTime("-Flush-", color);
            rinsing = false;
            rinsed = true;
          }
        } else {
          if (brewing) {
            cleanCount++;
            char message[7];
            int m = sprintf(message, "x%d %3ds", cleanCount, ms / 1000);
            //int m = sprintf(message, "%d/%d %2ds", cleanCount, cleanReps, ms / 1000);
            drawShotTime(message, color);
          } else {
            if (cleanCount == cleanReps) {
              if (rinsed) {
                drawShotTime("-Done-", TFT_GREEN);
                delay(3000);
                stopCleaning();
              } else {
                cleanCount = 0;
                drawShotTime("-Rinse-", color);
                rinsing = true;
              }
            } else {
              drawShotTime("-Wait-", color);
              waiting = true;
              delay(1000);
              waitStarted = millis();
            }
          }
        }
      } else {
        drawShotTime(String(ms / 1000) + "s", color);
      }
    }

    if (!sData.startsWith("sht") && brewing) {
      if (cleaning && !rinsing) {
        drawCleanTime(shotStarted);
      } else {
        drawShotTime(String((millis() - shotStarted) / 1000) + "s", TFT_YELLOW);
      }
    }
}

void connectToServer() {
  BLEClient*  pClient  = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)


  BLERemoteService* pRemoteService = pClient->getService(meCoffeeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(meCoffeeServiceUUID.toString().c_str());
    pClient->disconnect();

    connected = false;
    return;
  }

  Serial.println(" - Found our service");

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();

    connected = false;
    return;
  }
  Serial.println(" - Found our characteristic");

  if(pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
  Serial.println("Connected to meCoffee");
  wakeDisplay();
}

void sleepDisplay() {
  tft.fillScreen(TFT_BLACK); // Clear Screen
  digitalWrite(TFT_BL, !digitalRead(TFT_BL));
}

void wakeDisplay() {
  digitalWrite(TFT_BL, !digitalRead(TFT_BL));
}

void loop() {
  btn1.loop();
  btn2.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (!connected) {
      BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
      Serial.print("Devices found: ");
      Serial.println(foundDevices.getCount());
  
      Serial.println("Scan done!");
      pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
      delay(200);
    }
  
    if (doConnect == true) {
      Serial.println("Found meCoffee");
      connectToServer();
      doConnect = false;
    }
  }
}

void singleClick(Button2& btn) {
  if (!brewing && !cleaning) {
    if (btn == btn1) {
      enableWarmup();
    } else if (btn == btn2) {
      disableWarmup();
    }
  }
}

void longClickDetected(Button2& btn) {
  if (!brewing && !warmupRunning) {
    if (btn == btn1) {
      if (!cleaning) {
        startCleaning();
      } else {
        stopCleaning();
      }
    }
  }
}
