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
UPDATE_INTERVAL = 30 # Seconds

# --- PORTFOY BILGILERI ---
# Buraya Akbank'taki guncel lot sayinizi ve ortalama maliyetinizi yazin.
PORTFOLIO = [
    {"id": "THYAO", "sym": "THYAO.IS", "lot": 100, "cost": 315.38},
    {"id": "EFOR", "sym": "EFOR.IS", "lot": 500, "cost": 19.19},
    {"id": "ASELS", "sym": "ASELS.IS", "lot": 15, "cost": 404.56}
]

# --- IZLEME LISTESI (TUM 6 HISSE) ---
WATCHLIST = [
    {"id": "THYAO", "sym": "THYAO.IS"},
    {"id": "EFOR",  "sym": "EFOR.IS"},
    {"id": "ASELS", "sym": "ASELS.IS"},
    {"id": "ASTOR", "sym": "ASTOR.IS"},
    {"id": "DOFRB", "sym": "DOFRB.IS"},
    {"id": "PGSUS", "sym": "PGSUS.IS"}
]

def fetch_all_data():
    """Fetch ticker data for all unique symbols in Portfolio and Watchlist."""
    # Find unique symbols
    symbols = {}
    for item in PORTFOLIO:
        symbols[item["sym"]] = item["id"]
    for item in WATCHLIST:
        symbols[item["sym"]] = item["id"]
        
    cache = {}
    
    # Fetch from Yahoo Finance
    for sym_req, name in symbols.items():
        url = f"https://query1.finance.yahoo.com/v8/finance/chart/{sym_req}?range=6mo&interval=1d"
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})

        try:
            with urllib.request.urlopen(req, timeout=5) as response:
                data = json.loads(response.read().decode('utf-8'))
                result = data['chart']['result'][0]
                meta = result['meta']
                
                current_price = meta['regularMarketPrice']
                prev_close = meta.get('chartPreviousClose', current_price)
                
                # Gunluk % degisim
                daily_change = 0.0
                if prev_close and prev_close > 0:
                    daily_change = ((current_price - prev_close) / prev_close) * 100
                    
                # 6 Aylik % degisim
                six_mo_change = 0.0
                quotes = result['indicators']['quote'][0]
                if 'close' in quotes:
                    closes = [c for c in quotes['close'] if c is not None]
                    if closes:
                        six_mo_price = closes[0]
                        if six_mo_price > 0:
                            six_mo_change = ((current_price - six_mo_price) / six_mo_price) * 100
                            
                cache[sym_req] = {
                    "p": current_price,
                    "d": daily_change,
                    "m": six_mo_change
                }
                
        except Exception as e:
            print(f"Error fetching {name}: {e}")
            
        time.sleep(0.5) # API limits
        
    # --- 1. PORTFOY HESAPLAMASI ---
    port_data = []
    total_cost = 0.0
    total_value = 0.0
    
    for item in PORTFOLIO:
        c = cache.get(item["sym"])
        if c:
            val = c["p"] * item["lot"]
            cost = item["cost"] * item["lot"]
            total_value += val
            total_cost += cost
            
            port_data.append({
                "s": item["id"],
                "p": f"{c['p']:,.2f}",
                "d": round(c["d"], 1),
                "m": round(c["m"], 1)
            })
            
    port_pl = total_value - total_cost
    port_pl_pct = 0.0
    if total_cost > 0:
        port_pl_pct = (port_pl / total_cost) * 100
        
    port_summary = {
        "v": round(total_value, 2),
        "p": round(port_pl, 2),
        "c": round(port_pl_pct, 2)
    }
    
    # --- 2. IZLEME LISTESI HESAPLAMASI ---
    watch_data = []
    for item in WATCHLIST:
        c = cache.get(item["sym"])
        if c:
            watch_data.append({
                "s": item["id"],
                "p": f"{c['p']:,.2f}",
                "d": round(c["d"], 1),
                "m": round(c["m"], 1)
            })
            
    return port_data, port_summary, watch_data

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to Arduino on {SERIAL_PORT}")
        time.sleep(2)
    except serial.SerialException as e:
        print(f"Serial Port Error: {e}")
        return

    print("Starting Dual-Page BIST Ticker...")
    
    while True:
        port_data, port_summary, watch_data = fetch_all_data()
        
        if watch_data:
            # GONDERIM 1: PORTFOY SAYFASI (Page 1)
            payload1 = {"st": "OK", "page": 1, "d": port_data, "port": port_summary}
            ser.write((json.dumps(payload1) + "\n").encode('utf-8'))
            ser.flush()
            print(f"Sent Page 1 (Portfoy). Toplam: {port_summary['v']} TL")
            
            # Ekranda 10 saniye portfoy sayfasini tut
            time.sleep(10)
            
            # GONDERIM 2: WATCHLIST SAYFASI (Page 2)
            payload2 = {"st": "OK", "page": 2, "d": watch_data}
            ser.write((json.dumps(payload2) + "\n").encode('utf-8'))
            ser.flush()
            print(f"Sent Page 2 (Watchlist). 6 Hisse guncellendi.")
            
            # Ekranda 10 saniye watchlist sayfasini tut
            time.sleep(10)
        else:
            print("Failed to fetch data, retrying...")
            time.sleep(UPDATE_INTERVAL)
            
        # Toplamda 20 saniye gecti. Tekrar veri cekip donguye baslayacak.

if __name__ == "__main__":
    main()
