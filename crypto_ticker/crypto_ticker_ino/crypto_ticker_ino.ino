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

// Portfoy Verileri
float port_val = 0.0;
float port_pl = 0.0;
float port_pl_pct = 0.0;

// Paging logic
int currentPage = 0; // 0 = List, 1 = Portfolio
unsigned long lastPageChange = 0;
const unsigned long PAGE_DURATION = 10000; // 10 saniyede bir ekran degisir

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
  // 1. Read Serial Data
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
        
        // Portfoy verilerini cek
        port_val = doc["port"]["v"].as<float>();
        port_pl = doc["port"]["p"].as<float>();
        port_pl_pct = doc["port"]["c"].as<float>();
        
        // Yeni veri gelince guncel sayfayi hemen ciz
        if (status == "OK" && stockCount > 0) {
          renderCurrentPage();
        }
      }
    }
  }

  // 2. Handle Paging (Sayfa Degisimi)
  if (status == "OK" && stockCount > 0) {
    if (millis() - lastPageChange > PAGE_DURATION) {
      currentPage = (currentPage + 1) % 2; // 0 ve 1 arasinda gidip gelir
      lastPageChange = millis();
      renderCurrentPage();
    }
  }
}

void renderCurrentPage() {
  if (currentPage == 0) {
    renderList();
  } else {
    renderPortfolio();
  }
}

void renderList() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Tablo Başlığı
  display.setCursor(0, 0);
  display.print("HISSE");
  display.setCursor(50, 0);
  display.print("1G(%)");
  display.setCursor(95, 0);
  display.print("6A(%)");

  display.drawLine(0, 8, 128, 8, SH110X_WHITE);

  // Satır Satır Hisseler
  for (int i = 0; i < stockCount; i++) {
    int yPos = 10 + (i * 9); 
    
    display.setCursor(0, yPos);
    String sym = stocks[i].symbol;
    if(sym.length() > 5) sym = sym.substring(0,5);
    display.print(sym);

    display.setCursor(50, yPos);
    if (stocks[i].daily_change > 0) display.print("+");
    display.print(stocks[i].daily_change, 1);

    display.setCursor(95, yPos);
    if (stocks[i].six_mo_change > 0) display.print("+");
    display.print(stocks[i].six_mo_change, 1);
  }

  // Sayfa gostergesi (1. nokta dolu)
  display.fillCircle(118, 62, 1, SH110X_WHITE);
  display.drawCircle(124, 62, 1, SH110X_WHITE);

  display.display();
}

void renderPortfolio() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Baslik
  display.setCursor(25, 0);
  display.print("PORTFOY OZETI");
  display.drawLine(0, 9, 128, 9, SH110X_WHITE);
  
  // Bakiye (Guncel Deger)
  display.setCursor(0, 14);
  display.print("Guncel Deger:");
  
  display.setTextSize(2);
  display.setCursor(0, 26);
  display.print(port_val, 0); // Kusurat gosterme
  
  display.setTextSize(1);
  // Fiyatin sonuna TL yazdir (Y kordinatini ortalayarak)
  display.setCursor(display.getCursorX() + 2, 33);
  display.print("TL");
  
  // Kar Zarar
  display.setCursor(0, 46);
  display.print("Kar/Zarar: ");
  
  display.setCursor(0, 56);
  if (port_pl > 0) display.print("+");
  display.print(port_pl, 0);
  display.print(" TL (");
  if (port_pl_pct > 0) display.print("+");
  display.print(port_pl_pct, 1);
  display.print("%)");

  // Sayfa gostergesi (2. nokta dolu)
  display.drawCircle(118, 62, 1, SH110X_WHITE);
  display.fillCircle(124, 62, 1, SH110X_WHITE);

  display.display();
}