SUMMARY = "Camera MJPEG streaming server"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://stream.cpp \
           file://camera-stream.service"

DEPENDS = "libcamera libjpeg-turbo"

inherit systemd

SYSTEMD_SERVICE:${PN} = "camera-stream.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} \
        -o camera-stream ${UNPACKDIR}/stream.cpp \
        -I${STAGING_INCDIR}/libcamera \
        -lcamera -lcamera-base -ljpeg -lpthread \
        -std=c++17
}

do_install() {
    install -d ${D}/opt/camera
    install -m 0755 ${B}/camera-stream ${D}/opt/camera/camera-stream

    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${UNPACKDIR}/camera-stream.service \
        ${D}${systemd_unitdir}/system/camera-stream.service
}

FILES:${PN} = "/opt/camera/camera-stream ${systemd_unitdir}/system/camera-stream.service"

INSANE_SKIP:${PN}-dbg += "buildpaths"
INSANE_SKIP:${PN} += "buildpaths"
