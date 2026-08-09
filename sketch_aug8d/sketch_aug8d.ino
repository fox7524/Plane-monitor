#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoJson.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- TAPERED / SWEPT WING DOLGUN UÇAK İKONU (9x9 Piksel) ---
const unsigned char PROGMEM tapered_plane_bmp[] = {
  0b00001000, 0b00000000, //     *
  0b00001000, 0b00000000, //     *
  0b00011100, 0b00000000, //    ***
  0b01111111, 0b00000000, //  *******  (Kanat Kökü Kalın)
  0b00111110, 0b00000000, //   *****   (Tapered Kanat Ucu)
  0b00001000, 0b00000000, //     *     (Gövde)
  0b00011100, 0b00000000, //    ***    (Kuyruk)
  0b00001000, 0b00000000, //     *
  0b00000000, 0b00000000
};

struct Plane {
  String cs;
  int x;
  int y;
  int alt;
  int spd;
};

Plane planes[2];
int planeCount = 0;
String status = "INIT";

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
  display.setCursor(10, 25);
  display.println("FAST RADAR V2..");
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

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, jsonClean);

      if (!error) {
        status = doc["st"].as<String>();
        JsonArray arr = doc["p"].as<JsonArray>();

        planeCount = 0;
        for (JsonObject obj : arr) {
          if (planeCount < 2) {
            planes[planeCount].cs  = obj["cs"].as<String>();
            planes[planeCount].x   = obj["x"].as<int>();
            planes[planeCount].y   = obj["y"].as<int>();
            planes[planeCount].alt = obj["alt"].as<int>();
            planes[planeCount].spd = obj["spd"].as<int>();
            planeCount++;
          }
        }
        renderScreen();
      }
    }
  }
}

// PARSELLENMİŞ KARE IZGARA VE PARSEL METİNLERİ
void drawParsedGrid() {
  // 10 KM Dış Kare Sınırı
  display.drawRect(0, 0, 128, 64, SH110X_WHITE);

  // 6 KM İç Parsel Çizgisi (Noktalı)
  for (int x = 20; x < 108; x += 4) {
    display.drawPixel(x, 10, SH110X_WHITE);
    display.drawPixel(x, 54, SH110X_WHITE);
  }
  for (int y = 10; y < 54; y += 4) {
    display.drawPixel(20, y, SH110X_WHITE);
    display.drawPixel(108, y, SH110X_WHITE);
  }

  // 3 KM En İç Parsel Çizgisi (Noktalı)
  for (int x = 42; x < 86; x += 4) {
    display.drawPixel(x, 21, SH110X_WHITE);
    display.drawPixel(x, 43, SH110X_WHITE);
  }
  for (int y = 21; y < 43; y += 4) {
    display.drawPixel(42, y, SH110X_WHITE);
    display.drawPixel(86, y, SH110X_WHITE);
  }

  // Merkez Konumun (Senin Yerin)
  display.fillRect(63, 31, 3, 3, SH110X_WHITE);

  // Parsel Etiketleri (Köşe/Kenar Yazıları)
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print("10K");

  display.setCursor(22, 12);
  display.print("6K");

  display.setCursor(44, 23);
  display.print("3K");
}

void renderScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  // 1. Parsellenmiş Kare Izgarayı Çiz
  drawParsedGrid();

  // 2. Canlı Uçaklar ve Yapışık Bilgi Kutusu
  if (status == "OK" && planeCount > 0) {
    for (int i = 0; i < planeCount; i++) {
      int px = planes[i].x;
      int py = planes[i].y;

      // Tapered Uçak İkonunu Çiz (9x9 Piksel)
      display.drawBitmap(px - 4, py - 4, tapered_plane_bmp, 9, 9, SH110X_WHITE);

      // Yapışık Kutucuk Konumu
      int boxX = (px > 64) ? (px - 58) : (px + 8);
      int boxY = (py > 32) ? (py - 18) : (py + 4);

      // Kutucuk Çizgisi
      display.drawLine(px, py, (px > 64) ? boxX + 58 : boxX, boxY + 8, SH110X_WHITE);

      // Kutucuk Siyah Arka Plan & Çerçeve
      display.fillRect(boxX, boxY, 58, 17, SH110X_BLACK);
      display.drawRect(boxX, boxY, 58, 17, SH110X_WHITE);

      // Metinler: TK136 / 10054/450
      display.setTextSize(1);
      display.setCursor(boxX + 2, boxY + 2);
      display.print(planes[i].cs.substring(0, 7));

      display.setCursor(boxX + 2, boxY + 9);
      display.print(planes[i].alt);
      display.print("/");
      display.print(planes[i].spd);
    }
  } else if (status == "NO_PLANES") {
    display.setTextSize(1);
    display.setCursor(22, 38);
    display.println("N/A");
  }

  display.display();
}