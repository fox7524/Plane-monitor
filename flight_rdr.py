import time
import json
import serial
import math
import urllib.request
import urllib.error

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/cu.usbmodem1101'
BAUD_RATE = 115200

# Location Settings (Istanbul)
MY_LAT = 41.0082
MY_LON = 28.9784
MY_HEADING = 0    # Bilgisayarınızın Baktığı Yön (0=Kuzey, 90=Doğu, 180=Güney, 270=Batı)

# Radar Area Settings
# 128 pixels = 10 KM width -> 12.8 pixels per KM
# 64 pixels = 5 KM height
PIXELS_PER_KM = 12.8
FETCH_RADIUS_KM = 6.0  # 10x5 km rectangle max corner distance is ~5.6km

MAX_PLANES = 8    # Maximum number of planes to send to Arduino

# Screen Settings (OLED 128x64)
SCREEN_W = 128
SCREEN_H = 64
CENTER_X = SCREEN_W // 2
CENTER_Y = SCREEN_H // 2

def get_distance_km(lat1, lon1, lat2, lon2):
    """Calculate distance between two coordinates using Haversine formula."""
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (math.sin(dlat / 2)**2 + 
         math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2)**2)
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c

def calculate_pixel_coordinates(lat, lon):
    """Convert real-world coordinates to OLED screen pixel coordinates based on heading."""
    # Distances in km
    y_dist_km = (lat - MY_LAT) * 111.32
    x_dist_km = (lon - MY_LON) * 111.32 * math.cos(math.radians(MY_LAT))

    # Apply Heading Rotation
    # Clockwise rotation of axes = counter-clockwise rotation of the environment
    rad = math.radians(MY_HEADING)
    rot_x_km = x_dist_km * math.cos(rad) - y_dist_km * math.sin(rad)
    rot_y_km = x_dist_km * math.sin(rad) + y_dist_km * math.cos(rad)

    # Convert to pixels (X moves right +, Y moves forward/up -)
    x_pixel = int(CENTER_X + rot_x_km * PIXELS_PER_KM)
    y_pixel = int(CENTER_Y - rot_y_km * PIXELS_PER_KM)
    
    return x_pixel, y_pixel

def fetch_fr24_direct():
    """Fetch and parse live data from Flightradar24."""
    lat_change = FETCH_RADIUS_KM / 111.32
    lon_change = FETCH_RADIUS_KM / (111.32 * math.cos(math.radians(MY_LAT)))

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
                if key in ["full_count", "version"] or not isinstance(val, list) or len(val) <= 13:
                    continue

                lat = val[1]
                lon = val[2]
                alt_ft = val[4]
                speed_kts = val[5]
                callsign = val[16] if val[16] else val[13]
                callsign = callsign if callsign else "UNK"

                dist = get_distance_km(MY_LAT, MY_LON, lat, lon)
                if dist > FETCH_RADIUS_KM:
                    continue

                x_pixel, y_pixel = calculate_pixel_coordinates(lat, lon)

                # Filter out planes strictly outside our 10x5 km screen (with small buffer for icons)
                if not (-10 <= x_pixel <= SCREEN_W + 10 and -10 <= y_pixel <= SCREEN_H + 10):
                    continue

                planes.append({
                    "cs": callsign[:7].strip(),
                    "x": x_pixel,
                    "y": y_pixel,
                    "alt": alt_ft,
                    "spd": speed_kts,
                    "dist": dist
                })

            # Sort by distance to center and return top MAX_PLANES
            planes.sort(key=lambda p: p["dist"])
            return planes[:MAX_PLANES]

    except urllib.error.URLError as e:
        print(f"Network Error: {e}")
    except json.JSONDecodeError as e:
        print(f"Parse Error: {e}")
    except Exception as e:
        print(f"Unexpected Error: {e}")

    return []

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to Arduino on {SERIAL_PORT}")
        time.sleep(2) # Wait for Arduino to reset
    except serial.SerialException as e:
        print(f"Serial Port Error: {e}")
        return

    while True:
        planes = fetch_fr24_direct()
        payload = {
            "st": "OK" if planes else "NO_PLANES", 
            "p": planes
        }
        
        # Serialize and send
        json_str = json.dumps(payload) + "\n"
        ser.write(json_str.encode('utf-8'))
        ser.flush()

        print(f"Sent {len(planes)} planes to Arduino: {json_str.strip()}")
        time.sleep(2)

if __name__ == "__main__":
    main()