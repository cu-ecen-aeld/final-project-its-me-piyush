SUMMARY = "AI Rover"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://CMakeLists.txt \
    file://src/cbuf.c \
    file://src/logger.c \
    file://src/state_mgr.c \
    file://src/motor_ctrl.c \
    file://src/safety.c \
    file://src/decision.c \
    file://src/main.c \
    file://include/cbuf.h \
    file://include/logger.h \
    file://include/state.h \
    file://include/state_mgr.h \
    file://include/motor.h \
    file://include/safety.h \
    file://include/decision.h \
    file://rover-init \
    file://rover_web.py \
    file://llama_rover.py \
"



inherit cmake

DEPENDS = "libgpiod python3 python3-flask"
RDEPENDS:${PN} = "libgpiod python3 python3-flask"

EXTRA_OECMAKE = "-DCMAKE_BUILD_TYPE=Release"

do_install() {
    # Install rover binary
    install -d ${D}${bindir}
    install -m 0755 ${B}/rover ${D}${bindir}/rover

    # Install init script
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/rover-init ${D}${sysconfdir}/init.d/rover
    install -d ${D}${sysconfdir}/rc5.d
    ln -sf ../init.d/rover ${D}${sysconfdir}/rc5.d/S99rover
    install -d ${D}${sysconfdir}/rc3.d
    ln -sf ../init.d/rover ${D}${sysconfdir}/rc3.d/S99rover

    # Install Python scripts
    install -d ${D}/opt/ai-assistant
    install -m 0755 ${WORKDIR}/rover_web.py ${D}/opt/ai-assistant/rover_web.py
    install -m 0755 ${WORKDIR}/llama_rover.py ${D}/opt/ai-assistant/llama_rover.py

    # Install web UI init script
    install -d ${D}${sysconfdir}/rc5.d
}

FILES:${PN} += " \
    ${bindir}/rover \
    ${sysconfdir}/init.d/rover \
    ${sysconfdir}/rc5.d/S99rover \
    ${sysconfdir}/rc3.d/S99rover \
    /opt/ai-assistant/rover_web.py \
    /opt/ai-assistant/llama_rover.py \
"
