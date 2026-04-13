#!/bin/sh
LOG=/tmp/wifi-watchdog.log
FAIL_COUNT=0
MAX_FAILS=3

exec >> "$LOG" 2>&1

echo "=== wifi watchdog started $(date) ==="

while true; do
    sleep 60

    # Skip watchdog if we're already in AP mode
    if ps | grep -q '[h]ostapd'; then
        echo "$(date) — AP mode active, watchdog standing by"
        FAIL_COUNT=0
        continue
    fi

    # Check for valid IP on wlan0
    IP=$(ip addr show wlan0 2>/dev/null \
        | grep 'inet ' \
        | grep -v '169.254' \
        | awk '{print $2}' \
        | cut -d/ -f1)

    if [ -z "$IP" ]; then
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo "$(date) — no valid IP, fail $FAIL_COUNT/$MAX_FAILS"

        if [ "$FAIL_COUNT" -ge "$MAX_FAILS" ]; then
            echo "$(date) — WiFi lost, switching to AP mode"

            # Clean kill of wifi client
            killall wpa_supplicant 2>/dev/null
            killall udhcpc 2>/dev/null
            sleep 2

            # Remove marker so AP mode stays after next reboot too
            rm -f /etc/wifi-provisioned

            # Start AP
            /usr/bin/start-provision-ap.sh &
            FAIL_COUNT=0
        fi
    else
        if [ "$FAIL_COUNT" -gt 0 ]; then
            echo "$(date) — WiFi recovered, IP: $IP"
        fi
        FAIL_COUNT=0
    fi
done
