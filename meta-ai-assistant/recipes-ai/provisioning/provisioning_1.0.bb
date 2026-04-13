SUMMARY = "WiFi provisioning web portal"
DESCRIPTION = "Flask-free provisioning portal using Python built-ins"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://provision.py"



RDEPENDS:${PN} += "python3 python3-core"

do_install() {
    install -d ${D}/opt/ai-assistant
    install -m 0755 ${UNPACKDIR}/provision.py \
        ${D}/opt/ai-assistant/provision.py
}

FILES:${PN} += "/opt/ai-assistant/provision.py"
