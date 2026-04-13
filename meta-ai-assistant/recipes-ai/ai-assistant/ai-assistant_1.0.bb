SUMMARY = "AI Rover Assistant - UI server and AI bridge"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://ui_server.cpp \
           file://ai_bridge.py \
           file://ai-assistant.service \
           file://ai-bridge.service"

DEPENDS = "libcamera"
RDEPENDS:${PN} = "python3 python3-core camera-stream"

inherit systemd

SYSTEMD_SERVICE:${PN} = "ai-assistant.service ai-bridge.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} \
        -o ui-server ${UNPACKDIR}/ui_server.cpp \
        -lpthread -std=c++17
}

do_install() {
    install -d ${D}/opt/ai-assistant
    install -m 0755 ${B}/ui-server ${D}/opt/ai-assistant/ui-server
    install -m 0755 ${UNPACKDIR}/ai_bridge.py ${D}/opt/ai-assistant/ai_bridge.py

    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${UNPACKDIR}/ai-assistant.service \
        ${D}${systemd_unitdir}/system/ai-assistant.service
    install -m 0644 ${UNPACKDIR}/ai-bridge.service \
        ${D}${systemd_unitdir}/system/ai-bridge.service
}

FILES:${PN} = " \
    /opt/ai-assistant/ui-server \
    /opt/ai-assistant/ai_bridge.py \
    ${systemd_unitdir}/system/ai-assistant.service \
    ${systemd_unitdir}/system/ai-bridge.service \
"

INSANE_SKIP:${PN}-dbg += "buildpaths"
INSANE_SKIP:${PN} += "buildpaths"
