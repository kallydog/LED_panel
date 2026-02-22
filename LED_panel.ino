#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/FreeSans9pt7b.h>
#include "image.h"  // This file contains your pixel data

// 1. Define your Matrix Dimensions
#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

// 2. Define your Pin Map (Adjust these to match your physical wiring!)
#define R1_PIN 27
#define G1_PIN 26
#define B1_PIN 25
#define R2_PIN 13
#define G2_PIN 12
#define B2_PIN 14

#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN 18  // CRITICAL for 1/32 scan

#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

const char* ssid = "Louisnet";
const char* password = "Ganymede10";
const char* ha_temp_url = "http://192.168.0.230:8123/api/states/sensor.govee_climate_temperature";
const char* ha_lumi_url = "http://192.168.0.230:8123/api/states/sensor.0x0c2a6ffffe5c9d60_illuminance";
const char* ha_weather_url = "http://192.168.0.230:8123/api/states/weather.forecast_home";
const char* token = "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI3NjY1ZGQ4NGNhMTY0YzJiYjYwNTI1MjljMWQ5NGM0MyIsImlhdCI6MTc3MTU5NDQzOCwiZXhwIjoyMDg2OTU0NDM4fQ.CX11lvK__uwZOInMGYQbEGqPxeUs7LPwHkP1SIQ6y4s";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;         // Replace with your timezone offset (e.g., -18000 for EST)
const int daylightOffset_sec = 3600;  // 3600 if Daylight Savings is active, else 0

int lastMinute = -1;  // Initialized to -1 so it triggers a draw on startup
int lastSecond = -1;  // Initialized to -1 so it triggers a draw on startup
unsigned long lastWeatherUpdate = 60000;
const unsigned long weatherInterval = 60000;  // 60,000 milliseconds = 1 minute

MatrixPanel_I2S_DMA* dma_display = nullptr;

void setup() {
  // Sync time with NTP
  Serial.begin(115200);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Waiting for NTP time sync...");

  // 3. Configure the HUB75 DMA Structure
  //HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  HUB75_I2S_CFG mxconfig(64, 64, 1);

  // Map the pins defined above
  mxconfig.gpio.r1 = R1_PIN;
  mxconfig.gpio.g1 = G1_PIN;
  mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN;
  mxconfig.gpio.g2 = G2_PIN;
  mxconfig.gpio.b2 = B2_PIN;

  mxconfig.gpio.a = A_PIN;
  mxconfig.gpio.b = B_PIN;
  mxconfig.gpio.c = C_PIN;
  mxconfig.gpio.d = D_PIN;
  mxconfig.gpio.e = E_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe = OE_PIN;
  mxconfig.gpio.clk = CLK_PIN;

  // 2. Set the driver type using the suggested 'driver' member
  // We use SHIFTREG or FM6126A for many 64x64 panels to force proper addressing
  mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;
  // Use 'clkphase' (no underscore) as suggested by the compiler
  mxconfig.clkphase = false;
  // Use the numeric frequency directly since the constant isn't being recognized
  // 10MHz is usually represented as 10000000L
  // Try this specific name
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;


  // 4. Start the display
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(60);
  dma_display->clearScreen();

  // 2. WiFi Setup
  WiFi.begin(ssid, password);
  dma_display->setCursor(2, 25);
  dma_display->print("Connecting...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  dma_display->clearScreen();
}

// MAIN LOOP

void loop() {

  // Draw Time
  displayCustomTime();

  // --- 2. UPDATE WEATHER EVERY MINUTE ---
  unsigned long currentMillis = millis();


  if (currentMillis - lastWeatherUpdate >= weatherInterval) {
    // This code only runs once per minute
    Serial.println("Updating Weather Data from Home Assistant...");

    fetchHomeAssistantData();  // Your function that gets temp/rain/lux
    //updateWeatherDisplay();   // Your function that draws the icons

    lastWeatherUpdate = currentMillis;
  }
}


void fetchHomeAssistantData() {

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(ha_temp_url);
    http.addHeader("Authorization", token);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();

      // Use JsonDocument for Version 7 (or StaticJsonDocument<512> for V6)
      JsonDocument doc;
      deserializeJson(doc, payload);

      const char* state = doc["state"];
      const char* unit = doc["attributes"]["unit_of_measurement"];

      // --- DISPLAY DATA ---

      // Draw Indoor
      dma_display->setTextColor(dma_display->color565(255, 0, 0), dma_display->color565(0, 0, 0));  // red
      dma_display->setCursor(1, 19);
      dma_display->print("In  ");

      // Draw Value
      dma_display->setTextColor(dma_display->color565(255, 127, 0), dma_display->color565(0, 0, 0));  // White
      dma_display->print(state);

      // Manually draw that degree circle again!
      int x = dma_display->getCursorX();
      dma_display->drawRect(x, 19, 3, 3, dma_display->color565(255, 127, 0));
      dma_display->setCursor(x + 4, 19);
      dma_display->setTextColor(dma_display->color565(255, 127, 0), dma_display->color565(0, 0, 0));  // green for unit
      dma_display->print("C");
    }

    http.end();

    // Get Weather
    http.begin(ha_weather_url);
    http.addHeader("Authorization", token);
    httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);

      // 1. Get the condition (e.g., "sunny")
      const char* condition = doc["state"];

      // 2. Get the temperature from attributes
      float temp = doc["attributes"]["temperature"];
      // 2. Get the Wind Speed from attributes
      float windSpeed = doc["attributes"]["wind_speed"];
      // 3. Get the Unit (km/h, mph, or m/s)
      const char* unit = doc["attributes"]["wind_speed_unit"];
      int rainChance = doc["attributes"]["precipitation_amount"];
      // Inside your JSON parsing block
      float windBearing = doc["attributes"]["wind_bearing"];

      // Draw Outdoor Temp
      dma_display->setTextColor(dma_display->color565(0, 0, 255), dma_display->color565(0, 0, 0));  // White
      dma_display->setCursor(1, 30);
      dma_display->print("Out ");
      dma_display->setTextColor(dma_display->color565(255, 127, 0), dma_display->color565(0, 0, 0));  // orange for temp
      int displayTemp = (int)temp;                                                                    // Rounding to nearest whole number
      if (displayTemp < 10 && displayTemp >= 0) {
        dma_display->print(" ");  // Add the padding space
      }
      dma_display->print(temp, 1);  // 1 decimal place
      // Manually draw that degree C
      int x = dma_display->getCursorX();
      dma_display->drawRect(x, 30, 3, 3, dma_display->color565(255, 127, 0));
      dma_display->setCursor(x + 4, 30);
      dma_display->setTextColor(dma_display->color565(255, 127, 0), dma_display->color565(0, 0, 0));  // green for unit
      dma_display->print("C");

      // Draw wind speed & direction
      drawFullBoxArrow(1, 38, windBearing);
      dma_display->setCursor(18, 41);
      dma_display->setTextColor(dma_display->color565(0, 255, 0), dma_display->color565(0, 0, 0));
      dma_display->print(windSpeed, 1);  // 1 decimal place
      x = dma_display->getCursorX();
      dma_display->setCursor(x+1, 41);
      dma_display->print(unit);

      // Draw the icon
      if (String(condition) == "rainy" || String(condition) == "pouring") {
        dma_display->drawRGBBitmap(0, 49, icon_rainy, 16, 16);
      } else if (String(condition) == "lightning" || String(condition) == "lightning-rainy") {
        dma_display->drawRGBBitmap(0, 49, icon_thunder, 16, 16);
      } else if (String(condition) == "cloudy" || String(condition) == "partlycloudy") {
        dma_display->drawRGBBitmap(0, 49, icon_cloudy, 16, 16);
      } else if (String(condition) == "sunny" || String(condition) == "clear-night") {
        dma_display->drawRGBBitmap(0, 49, icon_sunny, 16, 16);
      } else if (String(condition) == "lightning") {
        dma_display->drawRGBBitmap(0, 49, icon_thunder, 16, 16);
      } else if (String(condition) == "lightning-rainy") {
        // You could also draw thunder here, or create a hybrid!
        dma_display->drawRGBBitmap(0, 49, icon_thunder, 16, 16);
      }


      // Condition text
      dma_display->setTextColor(dma_display->color565(255, 255, 0), dma_display->color565(0, 0, 0));  // Yellow for condition
      dma_display->setCursor(18, 52);
      dma_display->print(String(condition).substring(0,7));
    }
    http.end();

    // Get Illuminance
    http.begin(ha_lumi_url);
    http.addHeader("Authorization", token);
    httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);

      float lux = doc["state"].as<float>();

      dma_display->setBrightness8(lux/2.5 + 10);  // Dim for night
    }
  }
}


void displayCustomTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    Serial.print("\nno time\n");
    return;
  }

  // Check if the second has actually changed
  if (timeinfo.tm_sec == lastSecond) {
    return;
  }

  // Clear the 64x16 header
  //if (timeinfo.tm_min != lastMinute) {
  dma_display->fillRect(0, 0, 64, 16, 0);
  //}

  lastMinute = timeinfo.tm_min;  // Update the tracker
  lastSecond = timeinfo.tm_sec;  // Update the tracker

  // Prepare strings
  char hour[3];      // "12"
  char hourMins[6];  // "12"
  char min[3];       // "30"
  char amPm[3];      // "am" or "pm"
  const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  const char* day = days[timeinfo.tm_wday];

  strftime(hourMins, sizeof(hourMins), "%I:%M", &timeinfo);
  strftime(hour, sizeof(hour), "%I", &timeinfo);
  strftime(min, sizeof(min), "%M", &timeinfo);
  strftime(amPm, sizeof(amPm), "%P", &timeinfo);  // %P gives lowercase am/pm

  String h = String(hour);

  if (h.startsWith("0")) h.remove(0, 1);  // Remove leading zero

  // Draw the Numbers (Large Font)
  dma_display->setFont(&FreeSans9pt7b);
  dma_display->setTextColor(dma_display->color565(255, 255, 255), dma_display->color565(0, 0, 0));

  // Calculate width for centering
  int16_t x1, y1;
  uint16_t w1, h1;
  dma_display->getTextBounds(hourMins, 0, 0, &x1, &y1, &w1, &h1);

  // Offset startX slightly to leave room for am/pm on the right
  int startX = (64 - (w1 + 26)) / 2;
  dma_display->setCursor(1, 13);  // Y=13 keeps it inside the 16px height
  dma_display->print(h);
  if (timeinfo.tm_sec % 2 != 0) {
    dma_display->print(":");
  } else {
    dma_display->print(" ");
  }
  dma_display->print(min);

  // Draw am/pm (Small Font)
  // We get the current X position after printing the numbers
  int currentX = dma_display->getCursorX();
  dma_display->setFont();  // Revert to default tiny font
  dma_display->setTextSize(1);
  dma_display->setCursor(currentX + 2, 8);  // Set cursor higher up for am/pm
  dma_display->print(amPm);
  dma_display->setCursor(currentX + 2, 1);  // Set cursor higher up for am/pm
  dma_display->print(day);
}



void drawFullBoxArrow(int x, int y, float degrees) {
  // 1. Center of our 16x16 box
  float cx = x + 5.5;
  float cy = y + 5.5;

  // 2. Convert bearing to radians (0 is North)
  float angle = (degrees - 90) * 0.0174533;

  // 3. Calculate Tip and Tail (using radius of 6 to fill the 16x16 box)
  int xTip = cx + 6 * cos(angle);
  int yTip = cy + 6 * sin(angle);
  int xTail = cx - 6 * cos(angle);
  int yTail = cy - 6 * sin(angle);

  uint16_t white = dma_display->color565(0, 255, 255);

  dma_display->fillRect(x, y, 11, 11, 0);
  // 4. Draw the main shaft
  dma_display->drawLine(xTail, yTail, xTip, yTip, white);

  // 5. Calculate Arrowhead "Wings"
  float headAngle = 0.6;  // Width of the head
  int headLen = 5;        // Length of the wings

  int xL = xTip - headLen * cos(angle - headAngle);
  int yL = yTip - headLen * sin(angle - headAngle);
  int xR = xTip - headLen * cos(angle + headAngle);
  int yR = yTip - headLen * sin(angle + headAngle);

  // 6. Draw the head
  dma_display->drawLine(xTip, yTip, xL, yL, white);
  dma_display->drawLine(xTip, yTip, xR, yR, white);
}