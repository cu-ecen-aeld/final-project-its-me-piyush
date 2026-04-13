SUMMARY = "WiFi provisioning and management scripts"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://start-normal-wifi.sh \
    file://start-provision-ap.sh \
    file://net-mode-select.sh \
    file://wifi-watchdog.sh \
    file://net-mode \
    file://hostapd.conf \
    file://dnsmasq.conf \
"


RDEPENDS:${PN} += "wpa-supplicant hostapd dnsmasq iw"

do_install() {
    install -d ${D}/usr/bin
    install -m 0755 ${UNPACKDIR}/start-normal-wifi.sh  ${D}/usr/bin/
    install -m 0755 ${UNPACKDIR}/start-provision-ap.sh ${D}/usr/bin/
    install -m 0755 ${UNPACKDIR}/net-mode-select.sh    ${D}/usr/bin/
    install -m 0755 ${UNPACKDIR}/wifi-watchdog.sh      ${D}/usr/bin/

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${UNPACKDIR}/net-mode \
        ${D}${sysconfdir}/init.d/net-mode

    # Use distinct names to avoid conflicts with hostapd/dnsmasq defaults
    install -d ${D}${sysconfdir}
    install -m 0644 ${UNPACKDIR}/hostapd.conf \
        ${D}${sysconfdir}/hostapd-ap.conf
    install -m 0644 ${UNPACKDIR}/dnsmasq.conf \
        ${D}${sysconfdir}/dnsmasq-ap.conf

    install -d ${D}${sysconfdir}/rcS.d
    ln -sf ../init.d/net-mode \
        ${D}${sysconfdir}/rcS.d/S99net-mode

    printf '#!/bin/sh\nexit 0\n' > ${D}${sysconfdir}/rc.local
    chmod 0755 ${D}${sysconfdir}/rc.local
}

FILES:${PN} += " \
    /usr/bin/start-normal-wifi.sh \
    /usr/bin/start-provision-ap.sh \
    /usr/bin/net-mode-select.sh \
    /usr/bin/wifi-watchdog.sh \
    ${sysconfdir}/init.d/net-mode \
    ${sysconfdir}/rcS.d/S99net-mode \
    ${sysconfdir}/hostapd-ap.conf \
    ${sysconfdir}/dnsmasq-ap.conf \
    ${sysconfdir}/rc.local \
"

inherit systemd

SYSTEMD_SERVICE:${PN} = "net-mode.service"
SYSTEMD_AUTO_ENABLE = "enable"

SRC_URI += "file://net-mode.service"

do_install:append() {
    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${UNPACKDIR}/net-mode.service \
        ${D}${systemd_unitdir}/system/net-mode.service
}

FILES:${PN} += "${systemd_unitdir}/system/net-mode.service"
