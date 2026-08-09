#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ArduinoJson.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define MAX_PLANES 8
#define MAX_LABELS 3

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- MODERN SWEPT WING PLANE ICON (11x11 Pixels) ---
const unsigned char PROGMEM modern_plane_bmp[] = {
  0b00000100, 0b00000000, //      *
  0b00001110, 0b00000000, //     ***
  0b00011111, 0b00000000, //    *****
  0b00001110, 0b00000000, //     ***
  0b11001110, 0b01100000, // **  ***  **
  0b11101110, 0b11100000, // *** *** ***
  0b01111111, 0b11000000, //  *********
  0b00001110, 0b00000000, //     ***
  0b00001110, 0b00000000, //     ***
  0b00111111, 0b10000000, //   *******
  0b00000100, 0b00000000  //      *
};

struct Plane {
  String cs;
  int x;
  int y;
  int alt;
  int spd;
};

Plane planes[MAX_PLANES];
int planeCount = 0;
String status = "INIT";

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(150);

  if (!display.begin(0x3C, true)) {
    while (1); // Halt if display init fails
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 25);
  display.println("FAST RADAR V2.1");
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

      // Increased size to handle up to 8 planes
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonClean);

      if (!error) {
        status = doc["st"].as<String>();
        JsonArray arr = doc["p"].as<JsonArray>();

        planeCount = 0;
        for (JsonObject obj : arr) {
          if (planeCount < MAX_PLANES) {
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

// Draw the proportional radar grid (128x64 = 10km x 5km)
void drawRadarGrid() {
  // 1. Dış Çerçeve
  display.drawRect(0, 0, 128, 64, SH110X_WHITE);

  // 2. Merkez Haç (Crosshair) Noktalı Çizgiler
  for (int y = 0; y < 64; y += 4) display.drawPixel(64, y, SH110X_WHITE);
  for (int x = 0; x < 128; x += 4) display.drawPixel(x, 32, SH110X_WHITE);

  // 3. Mesafe Halkaları (1 KM = 12.8 Piksel)
  // 2 KM Yarıçap = 25.6 Piksel -> ~26
  display.drawCircle(64, 32, 26, SH110X_WHITE);
  
  // 4 KM Yarıçap = 51.2 Piksel -> ~51 (Alt/Üst kısımları ekrandan taşar, radar efekti verir)
  display.drawCircle(64, 32, 51, SH110X_WHITE);

  // 4. Merkez Nokta (Senin Konumun)
  display.fillRect(63, 31, 3, 3, SH110X_WHITE);

  // 5. Etiketler
  display.setTextSize(1);
  
  // 2K Etiketi (İç halkanın üstünde)
  display.setCursor(67, 7);
  display.print("2K");
  
  // 4K Etiketi (Dış halkanın sağında)
  display.setCursor(110, 35);
  display.print("4K");
}

// Draw plane icon and optional information box
void drawPlane(int index, Plane p) {
  // Draw Modern Plane Icon (11x11 Pixels, offset by 5 to center it)
  display.drawBitmap(p.x - 5, p.y - 5, modern_plane_bmp, 11, 11, SH110X_WHITE);

  // Only draw detailed label box for the closest MAX_LABELS planes
  if (index < MAX_LABELS) {
    // Determine box position to prevent going off-screen
    int boxX = (p.x > 64) ? (p.x - 58) : (p.x + 8);
    int boxY = (p.y > 32) ? (p.y - 18) : (p.y + 4);

    // Draw connector line
    display.drawLine(p.x, p.y, (p.x > 64) ? boxX + 58 : boxX, boxY + 8, SH110X_WHITE);

    // Box Background & Border
    display.fillRect(boxX, boxY, 58, 17, SH110X_BLACK);
    display.drawRect(boxX, boxY, 58, 17, SH110X_WHITE);

    // Texts: Callsign / Altitude/Speed
    display.setTextSize(1);
    display.setCursor(boxX + 2, boxY + 2);
    display.print(p.cs);

    display.setCursor(boxX + 2, boxY + 9);
    display.print(p.alt);
    display.print("/");
    display.print(p.spd);
  }
}

void renderScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  drawRadarGrid();

  if (status == "OK" && planeCount > 0) {
    // Draw planes in reverse order so the closest (index 0) are drawn last and on top
    for (int i = planeCount - 1; i >= 0; i--) {
      drawPlane(i, planes[i]);
    }
  } else if (status == "NO_PLANES") {
    display.setTextSize(1);
    display.setCursor(22, 38);
    display.println("N/A");
  }

  display.display();
}