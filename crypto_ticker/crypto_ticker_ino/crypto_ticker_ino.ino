#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoJson.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define MAX_COINS 4

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct Coin {
  String symbol;
  String price;
  float change;
};

Coin coins[MAX_COINS];
int coinCount = 0;
String status = "INIT";

// For scrolling/paging logic
int currentCoinIndex = 0;
unsigned long lastPageChange = 0;
const unsigned long PAGE_DURATION = 3000; // Show each coin for 3 seconds

// --- BITCOIN LOGO BITMAP (16x16) ---
const unsigned char PROGMEM btc_logo[] = {
  0b00000111, 0b11100000,
  0b00011000, 0b00011000,
  0b00100010, 0b10000100,
  0b01000010, 0b10000010,
  0b01000111, 0b11000010,
  0b10000010, 0b10100001,
  0b10000010, 0b10010001,
  0b10000011, 0b11100001,
  0b10000010, 0b00010001,
  0b10000010, 0b00010001,
  0b01000010, 0b00100010,
  0b01000111, 0b11000010,
  0b00100010, 0b10000100,
  0b00011000, 0b00011000,
  0b00000111, 0b11100000,
  0b00000000, 0b00000000
};

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(150);

  if (!display.begin(0x3C, true)) {
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(15, 25);
  display.println("CRYPTO TICKER");
  display.setCursor(15, 40);
  display.println("Waiting for data...");
  display.display();
}

void loop() {
  // 1. Read Serial Data
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    int startIdx = input.indexOf('{');
    int endIdx = input.lastIndexOf('}');

    if (startIdx != -1 && endIdx != -1 && endIdx > startIdx) {
      String jsonClean = input.substring(startIdx, endIdx + 1);

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, jsonClean);

      if (!error) {
        status = doc["st"].as<String>();
        JsonArray arr = doc["d"].as<JsonArray>();

        coinCount = 0;
        for (JsonObject obj : arr) {
          if (coinCount < MAX_COINS) {
            coins[coinCount].symbol = obj["s"].as<String>();
            coins[coinCount].price  = obj["p"].as<String>();
            coins[coinCount].change = obj["c"].as<float>();
            coinCount++;
          }
        }
      }
    }
  }

  // 2. Handle Paging and Rendering
  if (status == "OK" && coinCount > 0) {
    if (millis() - lastPageChange > PAGE_DURATION) {
      currentCoinIndex = (currentCoinIndex + 1) % coinCount;
      lastPageChange = millis();
    }
    renderCoin(currentCoinIndex);
  }
}

void renderCoin(int index) {
  display.clearDisplay();
  
  Coin c = coins[index];

  // Draw Header Line
  display.drawLine(0, 18, 128, 18, SH110X_WHITE);

  // Symbol Name (Large)
  display.setTextSize(2);
  display.setCursor(22, 2);
  display.print(c.symbol);

  // Optional: Draw BTC logo if it's BTC
  if (c.symbol == "BTC") {
    display.drawBitmap(2, 2, btc_logo, 16, 16, SH110X_WHITE);
  }

  // Price (Very Large)
  display.setTextSize(2);
  // Center price somewhat manually
  int px = (128 - (c.price.length() * 12)) / 2;
  if (px < 0) px = 0;
  display.setCursor(px, 26);
  display.print("$");
  display.print(c.price);

  // 24H Change (Bottom)
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.print("24H: ");
  
  if (c.change > 0) {
    display.print("+");
    // Draw an UP arrow
    display.fillTriangle(90, 56, 95, 51, 100, 56, SH110X_WHITE);
  } else if (c.change < 0) {
    // Draw a DOWN arrow
    display.fillTriangle(90, 51, 95, 56, 100, 51, SH110X_WHITE);
  }
  
  display.print(c.change);
  display.print("%");

  // Page Indicators (Dots at bottom right)
  for (int i = 0; i < coinCount; i++) {
    if (i == index) {
      display.fillCircle(110 + (i * 6), 54, 2, SH110X_WHITE);
    } else {
      display.drawCircle(110 + (i * 6), 54, 2, SH110X_WHITE);
    }
  }

  display.display();
}