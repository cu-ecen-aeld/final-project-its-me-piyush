#!/bin/sh
LOG=/tmp/provision-ap.log
exec >> "$LOG" 2>&1

echo "=== provision AP start $(date) ==="

killall wpa_supplicant 2>/dev/null
killall udhcpc 2>/dev/null
killall hostapd 2>/dev/null
killall dnsmasq 2>/dev/null
killall python3 2>/dev/null
rm -f /var/run/wpa_supplicant/wlan0

sleep 5

ip link set wlan0 down
sleep 1
ip addr flush dev wlan0
ifconfig wlan0 192.168.4.1 netmask 255.255.255.0 up
sleep 2

mkdir -p /var/run/hostapd
hostapd -B /etc/hostapd-ap.conf
sleep 3

killall dnsmasq 2>/dev/null
dnsmasq --conf-file=/etc/dnsmasq-ap.conf
sleep 1

echo "=== AP up, starting portal ==="
python3 /opt/ai-assistant/provision.py
