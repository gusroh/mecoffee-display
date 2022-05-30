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

static boolean timerEnabled = true;
static boolean timerRunning = false;
static int warmupTime;
static boolean warmupNeeded = false;

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
  btn1.setReleasedHandler(released);

  btn2.begin(BUTTON_2);
  btn2.setReleasedHandler(released);
  
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
    if (!timerEnabled) {
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

void enableTimer() {
  timerEnabled = true;
  Serial.println("Timer enabled");
}

void disableTimer() {
  timerEnabled = false;
  timerRunning = false;
  warmupNeeded = false;
  warmupTime = 0;
  drawShotTime("0s", TFT_LIGHTGREY);
  currentShotTime = "";
  Serial.println("Timer disabled");
}

void initTimer(float floatTemp) {
  if (floatTemp < 50) {
    warmupTime = 15 * 60; // 15 min
    warmupNeeded = true;
    timerRunning = true;
  } else if (floatTemp < 70) {
    warmupTime = 10 * 60; // 10 min
    warmupNeeded = true;
    timerRunning = true;
  } else if (floatTemp < 85) {
    warmupTime = 5 * 60; // 5 min
    warmupNeeded = true;
    timerRunning = true;
  } else {
    warmupTime = 0 * 60; // 0 min
    disableTimer();
  }
}

void updateTimer(int onTime) {
  if (warmupNeeded) {
  int warmupTimeDiff = warmupTime - onTime;
  if (warmupTimeDiff < 0) {
    disableTimer();
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

      if (timerEnabled) {
        if (!timerRunning) {
          initTimer(floatTemp);
        } else {
          updateTimer(onTime);
        }
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

      drawShotTime(String(ms / 1000) + "s", color);
    }

    if (!sData.startsWith("sht") && brewing) {
      drawShotTime(String((millis() - shotStarted) / 1000) + "s", TFT_YELLOW);
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

//  delay(2000);
}

void released(Button2& btn) {
  if (btn == btn1) {
    enableTimer();
  } else if (btn == btn2) {
    disableTimer();
  }
}
