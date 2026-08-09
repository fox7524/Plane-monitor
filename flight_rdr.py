import time
import json
import serial
import math
import urllib.request

SERIAL_PORT = '/dev/cu.usbmodem1101'
BAUD_RATE = 115200

# Tam Konumun (İstanbul)
MY_LAT = 41.0082
MY_LON = 28.9784
RADIUS_KM = 10.0  # Tam 10 KM

MY_HEADING = 0  # Bakış Yönün (0=Kuzey, 90=Doğu, 180=Güney, 270=Batı)

def get_distance_km(lat1, lon1, lat2, lon2):
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c

def fetch_fr24_direct():
    lat_change = RADIUS_KM / 111.0
    lon_change = RADIUS_KM / (111.0 * math.cos(math.radians(MY_LAT)))

    bounds = f"{MY_LAT + lat_change:.4f},{MY_LAT - lat_change:.4f},{MY_LON - lon_change:.4f},{MY_LON + lon_change:.4f}"
    url = f"https://data-cloud.flightradar24.com/zones/fcgi/feed.js?bounds={bounds}&faa=1&satellite=1&mlat=1&flarm=1&adsb=1&gnd=0&air=1&vehicles=0&estimated=1&stats=0"

    req = urllib.request.Request(
        url, 
        headers={'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)'}
    )

    try:
        with urllib.request.urlopen(req, timeout=3) as response:
            data = json.loads(response.read().decode('utf-8'))
            planes = []

            for key, val in data.items():
                if key in ["full_count", "version"]:
                    continue

                if isinstance(val, list) and len(val) > 13:
                    lat = val[1]
                    lon = val[2]
                    callsign = val[16] if val[16] else val[13]
                    alt_ft = val[4]
                    speed_kts = val[5]

                    if not callsign:
                        callsign = "UNK"

                    dist = get_distance_km(MY_LAT, MY_LON, lat, lon)
                    if dist > RADIUS_KM:
                        continue

                    lat_diff = lat - MY_LAT
                    lon_diff = lon - MY_LON

                    # Heading Dönüşü
                    rad = math.radians(MY_HEADING)
                    rot_lon = lon_diff * math.cos(rad) - lat_diff * math.sin(rad)
                    rot_lat = lon_diff * math.sin(rad) + lat_diff * math.cos(rad)

                    # 10 KM Piksel Dönüşümü
                    x_pixel = int(64 + (rot_lon / lon_change) * 58)
                    y_pixel = int(32 - (rot_lat / lat_change) * 26)

                    x_pixel = max(4, min(124, x_pixel))
                    y_pixel = max(4, min(60, y_pixel))

                    planes.append({
                        "cs": callsign[:7].strip(),
                        "x": x_pixel,
                        "y": y_pixel,
                        "alt": alt_ft,
                        "spd": speed_kts,
                        "dist": dist
                    })

            planes.sort(key=lambda p: p["dist"])
            return planes[:2]

    except Exception as e:
        print(f"Baglanti / Parse Hatasi: {e}")

    return []

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Flightradar24 Direkt Canli Akis Aktif: {SERIAL_PORT}")
        time.sleep(1)
    except Exception as e:
        print(f"Port Hatasi: {e}")
        return

    while True:
        planes = fetch_fr24_direct()
        payload = {"st": "OK" if planes else "NO_PLANES", "p": planes}
        json_str = json.dumps(payload) + "\r\n"

        ser.write(json_str.encode('utf-8'))
        ser.flush()

        print(f"FR24 Canli 10KM Ucaklar ({len(planes)} adet): {json_str.strip()}")
        time.sleep(2)

if __name__ == "__main__":
    main()