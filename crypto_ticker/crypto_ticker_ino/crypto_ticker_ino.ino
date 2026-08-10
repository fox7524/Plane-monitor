#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoJson.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define MAX_STOCKS 6

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct Stock {
  String symbol;
  String price;
  float daily_change;
  float six_mo_change;
};

Stock stocks[MAX_STOCKS];
int stockCount = 0;
String status = "INIT";
int currentPage = 1;

// Portfoy Verileri
float port_val = 0.0;
float port_pl = 0.0;
float port_pl_pct = 0.0;

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
  display.println("BIST PORTFOY");
  display.setCursor(15, 40);
  display.println("Veri bekleniyor...");
  display.display();
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    int startIdx = input.indexOf('{');
    int endIdx = input.lastIndexOf('}');

    if (startIdx != -1 && endIdx != -1 && endIdx > startIdx) {
      String jsonClean = input.substring(startIdx, endIdx + 1);

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonClean);

      if (!error) {
        status = doc["st"].as<String>();
        if (doc.containsKey("page")) {
          currentPage = doc["page"].as<int>();
        } else {
          currentPage = 1;
        }
        
        JsonArray arr = doc["d"].as<JsonArray>();

        stockCount = 0;
        for (JsonObject obj : arr) {
          if (stockCount < MAX_STOCKS) {
            stocks[stockCount].symbol = obj["s"].as<String>();
            stocks[stockCount].price  = obj["p"].as<String>();
            stocks[stockCount].daily_change = obj["d"].as<float>();
            stocks[stockCount].six_mo_change = obj["m"].as<float>();
            stockCount++;
          }
        }
        
        if (currentPage == 1) {
          port_val = doc["port"]["v"].as<float>();
          port_pl = doc["port"]["p"].as<float>();
          port_pl_pct = doc["port"]["c"].as<float>();
        }
        
        if (status == "OK" && stockCount > 0) {
          if (currentPage == 1) {
            renderPortfolio();
          } else {
            renderWatchlist();
          }
        }
      }
    }
  }
}

void renderPortfolio() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // 1. UST KISIM: PORTFOY OZETI (Y=0 ve Y=8)
  display.setCursor(0, 0);
  display.print("BAKIYE: ");
  display.print(port_val, 0);
  display.print(" TL");

  display.setCursor(0, 8);
  display.print("K/Z   : ");
  if (port_pl > 0) display.print("+");
  display.print(port_pl, 0);
  display.print(" (");
  if (port_pl_pct > 0) display.print("+");
  display.print(port_pl_pct, 1);
  display.print("%)");

  // Ayirici Cizgi
  display.drawLine(0, 16, 128, 16, SH110X_WHITE);

  // 2. ALT KISIM: HISSE LISTESI (Y=17'den basliyor)
  for (int i = 0; i < stockCount; i++) {
    int yPos = 17 + (i * 8); 

    display.setCursor(0, yPos);
    String sym = stocks[i].symbol;
    if(sym.length() > 5) sym = sym.substring(0,5);
    display.print(sym);

    display.setCursor(45, yPos);
    if (stocks[i].daily_change > 0) display.print("+");
    display.print(stocks[i].daily_change, 1);
    display.print("%");

    display.setCursor(88, yPos);
    if (stocks[i].six_mo_change > 0) display.print("+");
    display.print(stocks[i].six_mo_change, 1);
    display.print("%");
  }

  // Page Indicator
  display.drawPixel(120, 60, SH110X_WHITE);
  display.drawPixel(124, 60, SH110X_BLACK);

  display.display();
}

void renderWatchlist() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Baslik
  display.setCursor(0, 0);
  display.print(" GENEL PIYASA (6 H) ");
  display.drawLine(0, 9, 128, 9, SH110X_WHITE);

  // Hisse Listesi (Y=12'den basliyor, rahatca 6 tane sigar)
  for (int i = 0; i < stockCount; i++) {
    int yPos = 12 + (i * 8); 

    display.setCursor(0, yPos);
    String sym = stocks[i].symbol;
    if(sym.length() > 5) sym = sym.substring(0,5);
    display.print(sym);

    display.setCursor(45, yPos);
    if (stocks[i].daily_change > 0) display.print("+");
    display.print(stocks[i].daily_change, 1);
    display.print("%");

    display.setCursor(88, yPos);
    if (stocks[i].six_mo_change > 0) display.print("+");
    display.print(stocks[i].six_mo_change, 1);
    display.print("%");
  }

  // Page Indicator
  display.drawPixel(120, 60, SH110X_BLACK);
  display.drawPixel(124, 60, SH110X_WHITE);

  display.display();
}