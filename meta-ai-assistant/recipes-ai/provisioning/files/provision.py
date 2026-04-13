#!/usr/bin/env python3

import http.server
import urllib.parse
import os
import subprocess
import json

SETUP_PIN        = "1234"
PROVISION_MARKER = "/etc/wifi-provisioned"
WPA_CONF         = "/etc/wpa_supplicant.conf"
NETWORKS_FILE    = "/etc/known-networks.json"


# ─────────────────────────────────────────
# WiFi scanning
# ─────────────────────────────────────────

def scan_networks():
    """Scan for available WiFi networks, return list of dicts."""
    try:
        result = subprocess.run(
            ["iw", "dev", "wlan0", "scan"],
            capture_output=True, text=True, timeout=15
        )
        networks = []
        current = {}
        for line in result.stdout.splitlines():
            line = line.strip()
            if line.startswith("BSS "):
                if current.get("ssid"):
                    networks.append(current)
                current = {}
            elif line.startswith("SSID:"):
                ssid = line.split("SSID:", 1)[1].strip()
                if ssid:
                    current["ssid"] = ssid
            elif line.startswith("signal:"):
                try:
                    dbm = float(line.split("signal:", 1)[1].split()[0])
                    current["signal"] = dbm
                except Exception:
                    current["signal"] = -99.0
            elif "capability:" in line.lower():
                current["secure"] = "Privacy" in line
        if current.get("ssid"):
            networks.append(current)

        # Deduplicate by SSID, keep strongest signal
        seen = {}
        for n in networks:
            ssid = n["ssid"]
            if ssid not in seen or n.get("signal", -99) > seen[ssid].get("signal", -99):
                seen[ssid] = n
        networks = sorted(seen.values(), key=lambda x: x.get("signal", -99), reverse=True)
        return networks
    except Exception as e:
        print(f"Scan error: {e}")
        return []


def signal_bars(dbm):
    """Convert dBm to bar count 1-4."""
    if dbm >= -55:
        return 4
    elif dbm >= -67:
        return 3
    elif dbm >= -75:
        return 2
    else:
        return 1


def bars_html(count):
    """Return colored signal bar icons."""
    bars = ""
    for i in range(1, 5):
        color = "#00d4ff" if i <= count else "#333"
        bars += f'<span style="color:{color};font-size:18px;">&#9646;</span>'
    return bars


# ─────────────────────────────────────────
# Known networks (memory)
# ─────────────────────────────────────────

def load_known_networks():
    try:
        with open(NETWORKS_FILE, "r") as f:
            return json.load(f)
    except Exception:
        return {}


def save_known_network(ssid, psk):
    known = load_known_networks()
    known[ssid] = psk
    with open(NETWORKS_FILE, "w") as f:
        json.dump(known, f, indent=2)


def build_wpa_conf(known_networks):
    """Build wpa_supplicant.conf with all known networks."""
    lines = [
        "ctrl_interface=/var/run/wpa_supplicant",
        "update_config=1",
        "country=US",
        "",
    ]
    priority = len(known_networks)
    for ssid, psk in known_networks.items():
        lines += [
            "network={",
            f'    ssid="{ssid}"',
            f'    psk="{psk}"',
            "    scan_ssid=1",
            f"    priority={priority}",
            "}",
            "",
        ]
        priority -= 1
    return "\n".join(lines)


# ─────────────────────────────────────────
# HTML templates
# ─────────────────────────────────────────

STYLE = """
* { box-sizing: border-box; }
body {
  font-family: Arial, sans-serif;
  background: #1a1a2e; color: #eee;
  display: flex; justify-content: center;
  align-items: flex-start; min-height: 100vh;
  margin: 0; padding: 20px 0;
}
.card {
  background: #16213e; border-radius: 12px;
  padding: 28px 24px; width: 100%;
  max-width: 420px;
  box-shadow: 0 4px 24px rgba(0,0,0,0.4);
}
h2 { margin: 0 0 4px 0; font-size: 22px; color: #00d4ff; }
p.subtitle { margin: 0 0 20px 0; color: #888; font-size: 13px; }
.network-list { margin-bottom: 20px; }
.network-item {
  display: flex; align-items: center;
  justify-content: space-between;
  padding: 12px 14px; margin-bottom: 8px;
  background: #0f3460; border-radius: 8px;
  cursor: pointer; border: 2px solid transparent;
  transition: border-color 0.2s;
}
.network-item:hover { border-color: #00d4ff; }
.network-item.selected { border-color: #00d4ff; background: #1a4a7a; }
.network-name { font-size: 15px; font-weight: bold; }
.network-meta { font-size: 12px; color: #888; margin-top: 2px; }
.lock { color: #f0a500; margin-right: 6px; }
.network-right { display: flex; align-items: center; gap: 8px; }
.known-badge {
  font-size: 10px; background: #00d4ff;
  color: #1a1a2e; border-radius: 4px;
  padding: 2px 6px; font-weight: bold;
}
label { display: block; margin-bottom: 4px; font-size: 13px; color: #aaa; }
input {
  width: 100%; padding: 12px; margin-bottom: 14px;
  border-radius: 8px; border: 1px solid #0f3460;
  background: #0f3460; color: #eee; font-size: 15px;
}
button {
  width: 100%; padding: 13px; background: #00d4ff;
  color: #1a1a2e; border: none; border-radius: 8px;
  font-size: 16px; font-weight: bold; cursor: pointer;
}
button:hover { background: #00b8d9; }
button.secondary {
  background: #0f3460; color: #00d4ff;
  border: 1px solid #00d4ff; margin-top: 8px;
}
button.secondary:hover { background: #1a4a7a; }
button.danger {
  background: #ff4444; color: white;
  font-size: 13px; padding: 8px; margin-top: 6px;
}
.error {
  background: #ff4444; color: white;
  padding: 10px 14px; border-radius: 8px;
  margin-bottom: 14px; font-size: 13px;
}
.section-title {
  font-size: 12px; color: #888; text-transform: uppercase;
  letter-spacing: 1px; margin-bottom: 10px;
}
.divider {
  border: none; border-top: 1px solid #0f3460;
  margin: 18px 0;
}
.spinner {
  text-align: center; padding: 20px; color: #888;
}
.known-networks { margin-bottom: 16px; }
.known-item {
  display: flex; justify-content: space-between;
  align-items: center; padding: 8px 12px;
  background: #0f3460; border-radius: 8px;
  margin-bottom: 6px; font-size: 13px;
}
.known-item button {
  width: auto; padding: 4px 10px;
  font-size: 11px; background: #ff4444;
  color: white; margin: 0;
}
"""


def build_main_page(networks, known, error="", selected_ssid=""):
    known_ssids = list(known.keys())

    # Build network list HTML
    network_items = ""
    if not networks:
        network_items = '<div class="spinner">No networks found. <a href="/" style="color:#00d4ff">Refresh</a></div>'
    else:
        for n in networks:
            ssid      = n["ssid"]
            signal    = n.get("signal", -99)
            secure    = n.get("secure", True)
            bars      = signal_bars(signal)
            bars_icon = bars_html(bars)
            lock_icon = '<span class="lock">&#x1F512;</span>' if secure else ""
            known_badge = '<span class="known-badge">SAVED</span>' if ssid in known_ssids else ""
            selected  = 'selected' if ssid == selected_ssid else ''

            network_items += f"""
            <div class="network-item {selected}"
                 onclick="selectNetwork('{ssid}', {'true' if ssid in known_ssids else 'false'})">
              <div>
                <div class="network-name">{lock_icon}{ssid}</div>
                <div class="network-meta">{signal:.0f} dBm</div>
              </div>
              <div class="network-right">
                {known_badge}
                {bars_icon}
              </div>
            </div>"""

    # Build known networks management section
    known_list = ""
    if known_ssids:
        known_list = '<div class="section-title">Saved Networks</div><div class="known-networks">'
        for ssid in known_ssids:
            known_list += f"""
            <div class="known-item">
              <span>&#x1F4BE; {ssid}</span>
              <form method="POST" action="/forget" style="margin:0">
                <input type="hidden" name="ssid" value="{ssid}">
                <input type="hidden" name="pin" id="forget-pin-{ssid}" value="">
                <button type="submit"
                  onclick="this.form['forget-pin-{ssid}'].value=document.getElementById('pin').value">
                  Forget
                </button>
              </form>
            </div>"""
        known_list += "</div>"

    error_html = f'<div class="error">&#x26A0; {error}</div>' if error else ""

    return f"""<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AI Assistant Setup</title>
  <style>{STYLE}</style>
</head>
<body>
<div class="card">
  <h2>&#x1F916; AI Assistant</h2>
  <p class="subtitle">Select your WiFi network</p>

  {error_html}

  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;">
    <div class="section-title" style="margin:0">Available Networks</div>
    <a href="/" style="color:#00d4ff;font-size:12px;text-decoration:none;">&#x21BB; Refresh</a>
  </div>

  <div class="network-list">
    {network_items}
  </div>

  <hr class="divider">

  <form method="POST" action="/save" id="wifiForm">
    <label>Selected Network</label>
    <input name="ssid" id="ssid" type="text"
           placeholder="Tap a network above or type manually"
           value="{selected_ssid}" required>

    <label>Password</label>
    <input name="psk" id="psk" type="password"
           placeholder="WiFi password">

    <label>Setup PIN</label>
    <input name="pin" id="pin" type="password"
           placeholder="Enter setup PIN" required>

    <button type="submit">Save &amp; Connect</button>
    <button type="button" class="secondary"
            onclick="manualEntry()">Enter Manually</button>
  </form>

  <hr class="divider">
  {known_list}
</div>

<script>
function selectNetwork(ssid, isKnown) {{
  document.getElementById('ssid').value = ssid;
  document.querySelectorAll('.network-item').forEach(el => {{
    el.classList.remove('selected');
    if (el.querySelector('.network-name').textContent.trim()
        .replace('\\uD83D\\uDD12', '').trim() === ssid) {{
      el.classList.add('selected');
    }}
  }});
  if (isKnown) {{
    document.getElementById('psk').placeholder = 'Leave blank to use saved password';
  }} else {{
    document.getElementById('psk').placeholder = 'WiFi password';
  }}
}}

function manualEntry() {{
  document.getElementById('ssid').value = '';
  document.getElementById('ssid').focus();
}}
</script>
</body>
</html>"""


def build_success_page(ssid):
    return f"""<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AI Assistant Setup</title>
  <style>{STYLE}</style>
</head>
<body>
<div class="card" style="text-align:center;margin-top:60px;">
  <h2 style="color:#00ff88;">&#x2705; Saved!</h2>
  <p style="color:#aaa;font-size:14px;line-height:1.8;">
    Credentials saved for <strong>{ssid}</strong>.<br><br>
    The device is rebooting and will connect to your WiFi.<br><br>
    Disconnect from <em>AI-Assistant</em> and reconnect
    to your home network.<br>
    Find the device IP from your router to SSH in.
  </p>
</div>
</body>
</html>"""


# ─────────────────────────────────────────
# Request handler
# ─────────────────────────────────────────

class ProvisionHandler(http.server.BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        print(fmt % args)

    def do_GET(self):
        # Captive portal detection for iOS and Android
        captive_paths = ["/hotspot-detect.html", "/generate_204", "/connecttest.txt", "/ncsi.txt", "/success.txt"]
        if any(self.path.endswith(p) for p in captive_paths) or "captive.apple.com" in self.headers.get("Host", "") or "connectivitycheck" in self.headers.get("Host", ""):
            self.send_response(302)
            self.send_header("Location", "http://192.168.4.1/")
            self.end_headers()
            return
        networks = scan_networks()
        known    = load_known_networks()
        page     = build_main_page(networks, known)
        self._respond(200, page)

    def do_POST(self):
        body   = self._read_body()
        params = urllib.parse.parse_qs(body)
        path   = self.path

        pin = params.get("pin", [""])[0].strip()

        # ── Forget a saved network ──
        if path == "/forget":
            if pin != SETUP_PIN:
                self._show_error("Wrong PIN.")
                return
            ssid = params.get("ssid", [""])[0].strip()
            known = load_known_networks()
            if ssid in known:
                del known[ssid]
                with open(NETWORKS_FILE, "w") as f:
                    json.dump(known, f, indent=2)
                # Rebuild wpa_supplicant without forgotten network
                with open(WPA_CONF, "w") as f:
                    f.write(build_wpa_conf(known))
            networks = scan_networks()
            self._respond(200, build_main_page(networks, known))
            return

        # ── Save new network ──
        ssid = params.get("ssid", [""])[0].strip()
        psk  = params.get("psk",  [""])[0].strip()

        if pin != SETUP_PIN:
            self._show_error("Wrong PIN. Try again.", ssid)
            return

        if not ssid:
            self._show_error("Please select or enter a network name.", ssid)
            return

        # Load known networks
        known = load_known_networks()

        # If no password given, try to use saved password
        if not psk:
            if ssid in known:
                psk = known[ssid]
            else:
                self._show_error("Password required for new networks.", ssid)
                return

        # Save to known networks memory
        save_known_network(ssid, psk)
        known = load_known_networks()

        # Write wpa_supplicant.conf with ALL known networks
        with open(WPA_CONF, "w") as f:
            f.write(build_wpa_conf(known))

        # Write provisioned marker
        with open(PROVISION_MARKER, "w") as f:
            f.write("1\n")

        os.sync()

        self._respond(200, build_success_page(ssid))
        subprocess.Popen(["sh", "-c", "sleep 3 && reboot"])

    def _read_body(self):
        length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(length).decode()

    def _respond(self, code, html):
        self.send_response(code)
        self.send_header("Content-Type", "text/html")
        self.end_headers()
        self.wfile.write(html.encode())

    def _show_error(self, msg, selected=""):
        networks = scan_networks()
        known    = load_known_networks()
        page     = build_main_page(networks, known, error=msg, selected_ssid=selected)
        self._respond(200, page)


# ─────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────

if __name__ == "__main__":
    print("Provisioning portal at http://192.168.4.1:80")
    print(f"Setup PIN: {SETUP_PIN}")
    server = http.server.HTTPServer(("0.0.0.0", 80), ProvisionHandler)
    server.serve_forever()
