#!/bin/sh
LOG=/tmp/net-mode.log
exec >> "$LOG" 2>&1

echo "=== net-mode-select $(date) ==="

if [ -f /etc/wifi-provisioned ] && [ -f /etc/wpa_supplicant.conf ]; then
    echo "Provisioned — starting normal WiFi"
    /usr/bin/start-normal-wifi.sh
else
    echo "Not provisioned — starting AP portal"
    /usr/bin/start-provision-ap.sh
fi
