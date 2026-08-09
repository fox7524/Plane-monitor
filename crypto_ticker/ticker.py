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
# "lot" : Elinizdeki hisse adedi
# "cost": Hissenin size olan ortalama alis maliyeti
PORTFOLIO = [
    {"id": "THYAO", "sym": "THYAO.IS", "lot": 100, "cost": 285.50},
    {"id": "EFORC", "sym": "EFORC.IS", "lot": 500, "cost": 18.20},
    {"id": "ASELS", "sym": "ASELS.IS", "lot": 250, "cost": 55.00},
    {"id": "ASTOR", "sym": "ASTOR.IS", "lot": 150, "cost": 92.40},
    {"id": "DOFRB", "sym": "DOFRB.IS", "lot": 300, "cost": 50.10},
    {"id": "PGSUS", "sym": "PGSUS.IS", "lot": 50,  "cost": 850.00}
]

def fetch_stock_data():
    """Fetch ticker data and calculate portfolio value."""
    results = []
    
    total_cost = 0.0
    total_value = 0.0
    
    for item in PORTFOLIO:
        sym_req = item["sym"]
        name = item["id"]
        lot = item["lot"]
        avg_cost = item["cost"]
        
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
                
                # Portfoy Hesaplamalari
                item_total_cost = lot * avg_cost
                item_total_value = lot * current_price
                
                total_cost += item_total_cost
                total_value += item_total_value
                
                price_str = f"{current_price:,.2f}"
                
                results.append({
                    "s": name,
                    "p": price_str,
                    "d": round(daily_change, 1),
                    "m": round(six_mo_change, 1)
                })
                
        except Exception as e:
            print(f"Error fetching {name}: {e}")
            results.append({
                "s": name,
                "p": "ERR",
                "d": 0.0,
                "m": 0.0
            })
            
        time.sleep(0.5)
        
    # Genel Portfoy Kar/Zarar Hesaplamasi
    total_pl = total_value - total_cost
    total_pl_pct = 0.0
    if total_cost > 0:
        total_pl_pct = (total_pl / total_cost) * 100
        
    port_summary = {
        "v": round(total_value, 2),
        "p": round(total_pl, 2),
        "c": round(total_pl_pct, 2)
    }
        
    return results, port_summary

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to Arduino on {SERIAL_PORT}")
        time.sleep(2)
    except serial.SerialException as e:
        print(f"Serial Port Error: {e}")
        return

    print("Starting BIST Portfolio Ticker...")
    
    while True:
        stock_data, port_summary = fetch_stock_data()
        
        if stock_data:
            payload = {"st": "OK", "d": stock_data, "port": port_summary}
            json_str = json.dumps(payload) + "\n"
            
            ser.write(json_str.encode('utf-8'))
            ser.flush()
            
            print(f"Sent data. Toplam Portfoy: {port_summary['v']} TL | Kar/Zarar: {port_summary['p']} TL")
        else:
            print("Failed to fetch data, retrying...")
            
        time.sleep(UPDATE_INTERVAL)

if __name__ == "__main__":
    main()