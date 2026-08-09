# Plane-monitor

Bu proje, Flightradar24 üzerinden canlı uçuş verilerini çekerek, yakınınızdaki (varsayılan olarak 10 km yarıçapındaki) uçakları bir Arduino ve OLED ekran üzerinde gerçek zamanlı olarak görselleştirmenizi sağlayan donanım ve yazılım destekli bir radar sistemidir.

## Proje Yapısı

- `flight_rdr.py`: Flightradar24 API'sinden belirtilen koordinatlar etrafındaki uçuş verilerini alır. Uçakların merkeze olan uzaklıklarını hesaplar, ekran üzerindeki X ve Y piksel koordinatlarına dönüştürür ve bu verileri seri port üzerinden JSON formatında Arduino'ya gönderir.
- `sketch_aug8d/sketch_aug8d.ino`: Arduino tarafında çalışan koddur. Seri port üzerinden gelen JSON verisini ayrıştırır (parse eder) ve Adafruit_SH110X kütüphanesini kullanarak 128x64 çözünürlüğündeki OLED ekranda bir radar arayüzü çizer. Radarda uçak ikonu, uçuş numarası (callsign), yükseklik ve hız bilgileri gösterilir.

## Gerekli Donanımlar
- Arduino (Örn: Arduino Uno/Nano vb.)
- 128x64 OLED Ekran (I2C)
- Bağlantı kabloları

## Kurulum ve Kullanım

1. **Bağlantılar:** OLED ekranın SDA ve SCL pinlerini Arduino üzerinde belirtilen I2C pinlerine bağlayın.
2. **Yazılım (Arduino):** `sketch_aug8d.ino` dosyasını Arduino IDE ile açın, gerekli kütüphaneleri (Adafruit GFX, Adafruit SH110X, ArduinoJson) yükleyin ve kartınıza yükleyin.
3. **Yazılım (Python):** `flight_rdr.py` içindeki `SERIAL_PORT` değişkenini kendi bilgisayarınızdaki Arduino portuna göre güncelleyin. Ayrıca `MY_LAT` ve `MY_LON` değişkenlerini kendi konumunuza göre değiştirebilirsiniz. Gerekli Python kütüphanelerini (`pyserial`) yükledikten sonra script'i çalıştırın.

## Lisans
Bu proje **CC BY-NC 4.0** (Creative Commons Attribution-NonCommercial 4.0 International) lisansı ile lisanslanmıştır. Detaylar için `LICENSE` dosyasına bakabilirsiniz.
