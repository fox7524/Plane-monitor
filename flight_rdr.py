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
RADIUS_KM = 10.0  # Radar radius in KM
MY_HEADING = 0    # 0=North, 90=East, 180=South, 270=West

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

def calculate_pixel_coordinates(lat, lon, lat_change, lon_change):
    """Convert real-world coordinates to OLED screen pixel coordinates based on heading."""
    lat_diff = lat - MY_LAT
    lon_diff = lon - MY_LON

    # Apply Heading Rotation
    rad = math.radians(MY_HEADING)
    rot_lon = lon_diff * math.cos(rad) - lat_diff * math.sin(rad)
    rot_lat = lon_diff * math.sin(rad) + lat_diff * math.cos(rad)

    # Convert to 10 KM scale pixels
    x_pixel = int(CENTER_X + (rot_lon / lon_change) * (CENTER_X - 6))
    y_pixel = int(CENTER_Y - (rot_lat / lat_change) * (CENTER_Y - 6))

    # Keep within screen bounds
    x_pixel = max(4, min(SCREEN_W - 4, x_pixel))
    y_pixel = max(4, min(SCREEN_H - 4, y_pixel))
    
    return x_pixel, y_pixel

def fetch_fr24_direct():
    """Fetch and parse live data from Flightradar24."""
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
                if key in ["full_count", "version"] or not isinstance(val, list) or len(val) <= 13:
                    continue

                lat = val[1]
                lon = val[2]
                alt_ft = val[4]
                speed_kts = val[5]
                callsign = val[16] if val[16] else val[13]
                callsign = callsign if callsign else "UNK"

                dist = get_distance_km(MY_LAT, MY_LON, lat, lon)
                if dist > RADIUS_KM:
                    continue

                x_pixel, y_pixel = calculate_pixel_coordinates(lat, lon, lat_change, lon_change)

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