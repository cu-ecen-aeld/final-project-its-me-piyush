#!/bin/sh
LOG=/tmp/wifi-normal.log
exec >> "$LOG" 2>&1

echo "=== normal wifi start $(date) ==="

killall wpa_supplicant 2>/dev/null
killall udhcpc 2>/dev/null
killall hostapd 2>/dev/null
killall dnsmasq 2>/dev/null
rm -f /var/run/wpa_supplicant/wlan0

sleep 3
ip link set wlan0 up
wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf
sleep 8

udhcpc -i wlan0 -t 10 -T 3 -n
STATUS=$?

if [ $STATUS -eq 0 ]; then
    IP=$(ifconfig wlan0 | grep 'inet addr' | awk '{print $2}' | cut -d: -f2)
    echo "Connected successfully, IP: $IP"
else
    echo "WiFi connection failed (status $STATUS) — falling back to AP mode"
    rm -f /etc/wifi-provisioned
    /usr/bin/start-provision-ap.sh
fi
