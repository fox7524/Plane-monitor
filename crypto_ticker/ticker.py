import time
import json
import serial
import urllib.request
import urllib.error
import ssl

# Mac OS X Python SSL Fix
try:
    _create_unverified_https_context = ssl._create_unverified_context
except AttributeError:
    pass
else:
    ssl._create_default_https_context = _create_unverified_https_context

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/cu.usbmodem1101'
BAUD_RATE = 115200

# Coins to track (Binance symbols)
COINS = ["BTCUSDT", "ETHUSDT", "SOLUSDT", "AVAXUSDT"]
UPDATE_INTERVAL = 3 # Seconds

def fetch_crypto_data():
    """Fetch 24h ticker data from Binance API."""
    url = f"https://api.binance.com/api/v3/ticker/24hr?symbols={json.dumps(COINS).replace(' ', '')}"
    
    req = urllib.request.Request(
        url, 
        headers={'User-Agent': 'Mozilla/5.0'}
    )

    try:
        with urllib.request.urlopen(req, timeout=3) as response:
            data = json.loads(response.read().decode('utf-8'))
            results = []
            
            for item in data:
                symbol = item['symbol'].replace("USDT", "")
                price = float(item['lastPrice'])
                change = float(item['priceChangePercent'])
                
                # Format price beautifully
                if price >= 1000:
                    price_str = f"{price:,.0f}"
                elif price >= 10:
                    price_str = f"{price:,.2f}"
                else:
                    price_str = f"{price:,.4f}"
                    
                results.append({
                    "s": symbol,
                    "p": price_str,
                    "c": round(change, 2)
                })
                
            return results
            
    except Exception as e:
        print(f"API Error: {e}")
        return []

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to Arduino on {SERIAL_PORT}")
        time.sleep(2) # Wait for Arduino reset
    except serial.SerialException as e:
        print(f"Serial Port Error: {e}")
        return

    print("Starting Crypto Ticker...")
    
    while True:
        crypto_data = fetch_crypto_data()
        
        if crypto_data:
            payload = {"st": "OK", "d": crypto_data}
            json_str = json.dumps(payload) + "\n"
            
            ser.write(json_str.encode('utf-8'))
            ser.flush()
            
            print(f"Sent {len(crypto_data)} coins -> {crypto_data[0]['s']}: ${crypto_data[0]['p']}")
        else:
            print("Failed to fetch data, retrying...")
            
        time.sleep(UPDATE_INTERVAL)

if __name__ == "__main__":
    main()