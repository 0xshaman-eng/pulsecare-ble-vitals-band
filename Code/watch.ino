#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------- CONFIGURATION ----------
#define BUTTON_PIN 4 
#define SHORT_PRESS_TIME 600
#define LONG_PRESS_TIME 2000

#define I2C_SDA 6
#define I2C_SCL 7

// How often to send data to phone (and update "Real" average)
#define BLE_UPDATE_INTERVAL 2000  // Changed to 2s so real data is snappier

// SIMULATION SETTINGS
// The specific values you requested for the "Searching" phase
const int SIM_SEARCH_VALUES[] = {84, 66, 78, 82, 68, 75, 81, 69, 73, 80};
#define SIM_UPDATE_SPEED 1500     // Change fake number every 1.5 seconds

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- BITMAPS ----------
const unsigned char PROGMEM heart_bmp[] = {
  0b00000000, 0b00000000, 0b00011011, 0b00000000, 0b00111111, 0b10000000,
  0b01111111, 0b11000000, 0b01111111, 0b11000000, 0b01111111, 0b11000000,
  0b00111111, 0b10000000, 0b00011111, 0b00000000, 0b00001110, 0b00000000,
  0b00000100, 0b00000000
};

const unsigned char PROGMEM big_heart_bmp[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x0F, 0x00, 0x03, 0xFC, 0x3F, 0xC0, 0x07, 0xFE, 0x7F, 0xE0,
  0x0F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF8, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x7F, 0xFF, 0xFF, 0xFE, 0x7F, 0xFF, 0xFF, 0xFE, 0x7F, 0xFF, 0xFF, 0xFE, 0x7F, 0xFF, 0xFF, 0xFE,
  0x7F, 0xFF, 0xFF, 0xFE, 0x7F, 0xFF, 0xFF, 0xFE, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC,
  0x1F, 0xFF, 0xFF, 0xF8, 0x0F, 0xFF, 0xFF, 0xF0, 0x07, 0xFF, 0xFF, 0xE0, 0x03, 0xFF, 0xFF, 0xC0,
  0x01, 0xFF, 0xFF, 0x80, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x7F, 0xFE, 0x00, 0x00, 0x3F, 0xFC, 0x00,
  0x00, 0x1F, 0xF8, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x03, 0xC0, 0x00,
  0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00
};

enum ScreenMode {
  SHOW_VITALS,
  SHOW_TIME,
  SHOW_SOS_CONFIRM
};

// ---------- VARIABLES ----------
PulseOximeter pox;
ScreenMode screenMode = SHOW_VITALS;

long sumHR = 0;
long sumSpO2 = 0;
int sampleCount = 0;

int displayHR = 0;
int displaySpO2 = 0;

unsigned long lastBeatTime = 0;
unsigned long lastBleUpdate = 0;
unsigned long lastScreenUpdate = 0;

// Simulation Variables
unsigned long lastValidReadTime = 0; 
unsigned long lastSimUpdate = 0; // Tracks when we last changed the fake number
int simulationPhaseCount = 0;    // Counts how many fake readings we've shown

// BLE
BLECharacteristic *dataChar;
BLECharacteristic *sosChar;
BLECharacteristic *timeChar;

// Button & Time
unsigned long buttonDownTime = 0;
bool buttonPressed = false;
unsigned long sosScreenStartTime = 0; 
int hour_ = 12, min_ = 0, sec_ = 0;
int day_ = 1, month_ = 1, year_ = 2025;
unsigned long lastClockTick = 0;

// ---------- CALLBACKS ----------
void onBeatDetected() {
  lastBeatTime = millis();
}

class ServerCB : public BLEServerCallbacks {
  void onDisconnect(BLEServer*) {
    BLEDevice::startAdvertising();
  }
};

class TimeCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    String t = c->getValue().c_str();
    sscanf(t.c_str(), "%d:%d:%d,%d/%d/%d", &hour_, &min_, &sec_, &day_, &month_, &year_);
    Serial.println("Time Updated via BLE");
  }
};

// ---------- SETUP ----------
void setup() {
  // 1. Hardware Startup Delay
  delay(3000);

  Serial.begin(115200);
  Serial.println("Starting PulseCare...");
  
  // 2. Initialize I2C 
  Wire.begin(I2C_SDA, I2C_SCL); 
  Wire.setClock(100000); 

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Init Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.setTextColor(SSD1306_WHITE);
  
  // --- ANIMATION PHASE 1: HEARTBEAT ---
  for(int i=0; i<3; i++) { 
    display.clearDisplay();
    display.drawBitmap(48, 16, big_heart_bmp, 32, 32, 1);
    display.display();
    delay(400); 
    
    display.clearDisplay();
    display.display();
    delay(200); 
  }

  // --- ANIMATION PHASE 2: BRANDING & MOTTO ---
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10); display.println("PulseCare");

  display.setTextSize(1);
  display.setCursor(22, 35); display.println("Peace of mind,");
  display.setCursor(32, 48); display.println("every beat.");
  display.display();
  
  delay(3000); 

  // Show "Initializing" briefly
  display.clearDisplay();
  display.setCursor(20, 30); display.println("Initializing...");
  display.display();

  // 3. Init Sensor
  bool sensorFound = false;
  for(int i=0; i<5; i++) {
      if (pox.begin()) {
          sensorFound = true;
          break;
      }
      delay(500);
  }

  if (!sensorFound) {
    Serial.println("MAX30100 FAILED");
    display.clearDisplay();
    display.setCursor(0,0); display.println("SENSOR ERROR");
    display.display();
  } else {
    Serial.println("MAX30100 Initialized!");
    pox.setIRLedCurrent(MAX30100_LED_CURR_27_1MA); 
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }

  // Init BLE
  BLEDevice::init("PulseCare");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *service = server->createService("ABF0");
  
  dataChar = service->createCharacteristic("ABF2", BLECharacteristic::PROPERTY_NOTIFY);
  dataChar->addDescriptor(new BLE2902());
  sosChar = service->createCharacteristic("ABF1", BLECharacteristic::PROPERTY_NOTIFY);
  timeChar = service->createCharacteristic("ABF3", BLECharacteristic::PROPERTY_WRITE);
  timeChar->setCallbacks(new TimeCB());
  
  service->start();
  BLEDevice::startAdvertising();
  Serial.println("BLE Started");
}

// ---------- DEMO & LOGIC ----------
int forceToRange(int val, int minVal, int maxVal) {
  if (val < minVal) return minVal + random(0, 4);
  if (val > maxVal) return maxVal - random(0, 4);
  return val;
}

void collectSamples() {
  int rawHr = pox.getHeartRate();
  int rawSpo2 = pox.getSpO2();

  // 1. REAL FINGER DETECTED?
  if (rawHr > 30 && rawSpo2 > 50) {
      // Yes! Use Real Data
      lastValidReadTime = millis(); 
      simulationPhaseCount = 0; // Reset simulation
      
      int cleanHr = forceToRange(rawHr, 60, 90); 
      int cleanSpo2 = forceToRange(rawSpo2, 95, 99);
      
      sumHR += cleanHr;
      sumSpO2 += cleanSpo2;
      sampleCount++;
  } 
  else {
      // 2. NO FINGER - CHECK TIMEOUT
      if (millis() - lastValidReadTime > 6000) { 
           // We are in SIMULATION MODE
           
           // We do NOT add to sumHR here. We update displayHR directly in updateSimulation()
           // This prevents averaging out the randomness.
      }
  }
}

// NEW FUNCTION: Handles the fake values separately from real sampling
void updateSimulation() {
  // Only run if we are in "Simulation Mode" (6s timeout passed)
  if (millis() - lastValidReadTime > 6000) {
      
      // Only change the number every 1.5 seconds so user can see it
      if (millis() - lastSimUpdate > SIM_UPDATE_SPEED) {
          
          int fakeHr;
          
          // PHASE 1: First 5 readings (From your specific list)
          if (simulationPhaseCount < 5) {
              // Pick random index 0-9
              int randomIndex = random(0, 10);
              fakeHr = SIM_SEARCH_VALUES[randomIndex];
          } 
          // PHASE 2: After 5 readings (Random 70-80)
          else {
              fakeHr = 65 + random(0, 20); // 70 to 80
          }

          int fakeSpo2 = 96 + random(0, 3);
          
          // DIRECTLY UPDATE DISPLAY VARIABLES
          displayHR = fakeHr;
          displaySpO2 = fakeSpo2;
          
          simulationPhaseCount++;
          lastSimUpdate = millis();
          
          // Fake a beat for the heart animation
          lastBeatTime = millis(); 
          
          // Send to phone immediately so app updates too
          uint8_t d[2] = { (uint8_t)displayHR, (uint8_t)displaySpO2 };
          dataChar->setValue(d, 2);
          dataChar->notify();
      }
  }
}

void updateBLE() {
  // Only run this logic if we have REAL samples
  // If we are simulating, updateSimulation() handles the display
  if (sampleCount > 0) {
    displayHR = sumHR / sampleCount;
    displaySpO2 = sumSpO2 / sampleCount;

    sumHR = 0;
    sumSpO2 = 0;
    sampleCount = 0;
    
    uint8_t d[2] = { (uint8_t)displayHR, (uint8_t)displaySpO2 };
    dataChar->setValue(d, 2);
    dataChar->notify();
  } 
  else {
    // If no samples AND we are NOT simulating yet (in the 6s waiting period)
    if (millis() - lastValidReadTime <= 6000) {
       displayHR = 0;
       displaySpO2 = 0;
       
       uint8_t d[2] = { 0, 0 };
       dataChar->setValue(d, 2);
       dataChar->notify();
    }
  }
}

void drawScreen() {
  display.clearDisplay();

  if (screenMode == SHOW_SOS_CONFIRM) {
      display.setTextSize(2);
      display.setCursor(20, 15); display.println("SOS");
      display.setCursor(20, 35); display.println("SENT!");
      display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  } 
  else if (screenMode == SHOW_VITALS) {
      display.drawLine(0, 20, 128, 20, SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 5); display.print("PulseCare");
      
      display.setTextSize(2);
      display.setCursor(0, 28);
      display.print("HR: "); 
      if (displayHR == 0) display.print("--");
      else display.print(displayHR);

      display.setCursor(0, 48);
      display.print("O2: "); 
      if (displaySpO2 == 0) display.print("--");
      else display.print(displaySpO2);
      display.print("%");
      
      if (millis() - lastBeatTime < 150) {
         display.drawBitmap(100, 5, heart_bmp, 10, 10, 1);
      }
  } 
  else if (screenMode == SHOW_TIME) {
      display.setTextSize(2);
      display.setCursor(34, 20); display.printf("%02d:%02d", hour_, min_);
      display.setTextSize(1);
      display.setCursor(34, 40); display.printf("%02d/%02d/%04d", day_, month_, year_);
      
      display.drawRect(10, 55, 108, 6, SSD1306_WHITE); 
      int barWidth = map(sec_, 0, 60, 0, 104);
      display.fillRect(12, 57, barWidth, 2, SSD1306_WHITE);
  }
  display.display();
}

void handleButton() {
  bool state = digitalRead(BUTTON_PIN);
  if (!state && !buttonPressed) {
    buttonDownTime = millis();
    buttonPressed = true;
  }
  if (state && buttonPressed) {
    unsigned long dt = millis() - buttonDownTime;
    buttonPressed = false;
    
    if (dt < SHORT_PRESS_TIME) {
      if (screenMode != SHOW_SOS_CONFIRM) {
         screenMode = (screenMode == SHOW_VITALS) ? SHOW_TIME : SHOW_VITALS;
         drawScreen();
      }
    } 
    else if (dt > LONG_PRESS_TIME) {
      sosChar->setValue("SOS");
      sosChar->notify();
      screenMode = SHOW_SOS_CONFIRM;
      sosScreenStartTime = millis(); 
      drawScreen();
    }
  }
}

// ---------- MAIN LOOP ----------
void loop() {
  handleButton();
  pox.update(); 

  // 1. Collect Real Samples (Fast)
  static unsigned long lastSample = 0;
  if (millis() - lastSample > 50) {
     collectSamples(); 
     lastSample = millis();
  }

  // 2. Handle Simulation Updates (if needed)
  updateSimulation();

  // 3. BLE & Average Update (For REAL data only)
  if (millis() - lastBleUpdate > BLE_UPDATE_INTERVAL) {
    updateBLE();
    lastBleUpdate = millis();
  }

  // 4. Screen Refresh
  if (millis() - lastScreenUpdate > 100) {
    drawScreen();
    lastScreenUpdate = millis();
  }

  // 5. Clock Tick
  if (millis() - lastClockTick >= 1000) {
    sec_++;
    if (sec_ >= 60) { sec_ = 0; min_++; }
    if (min_ >= 60) { min_ = 0; hour_++; }
    lastClockTick = millis();
  }
  
  if (screenMode == SHOW_SOS_CONFIRM && millis() - sosScreenStartTime > 3000) {
      screenMode = SHOW_VITALS; 
  }
}
